/*
 * Crystal Mud Client
 * Copyright (C) Abigail Brady, Paul Lettington, Owen Cliffe, Stuart Brady
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifdef HAVE_LIBSSH2

#include <libssh2.h>

#include <asio.hpp>

#include <cctype>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <functional>

#include "Runtime.h"
#include "telnet.h"
#include "io.h"

using asio::ip::tcp;

// ---------------------------------------------------------------------------
// In-band password prompt detection for SSH shell sessions.
//
// SSH has no analogue to telnet's IAC WILL/WONT ECHO, so when a remote
// shell command like passwd(1) turns off PTY echo, the client sees no
// signal. Match common prompt tails in the server's output instead:
// set never_echo on match, clear on the next newline received from the
// server, and auto-clear after a timeout so a missed newline can't
// permanently wedge input display.
// ---------------------------------------------------------------------------

static constexpr size_t kSshTailMax = 48;
static constexpr auto kNeverEchoTimeout = std::chrono::seconds(60);

static bool tail_ends_with_ci(const std::string &tail, const char *needle)
{
    size_t nlen = std::strlen(needle);
    if (tail.size() < nlen) return false;
    const char *t = tail.c_str() + (tail.size() - nlen);
    for (size_t i = 0; i < nlen; i++) {
        if (std::tolower((unsigned char)t[i]) !=
            std::tolower((unsigned char)needle[i]))
            return false;
    }
    return true;
}

static bool looks_like_password_prompt(const std::string &tail)
{
    // Trim trailing spaces so "Password: " and "Password:" both match.
    size_t end = tail.size();
    while (end > 0 && tail[end - 1] == ' ') end--;
    std::string trimmed = tail.substr(0, end);
    return tail_ends_with_ci(trimmed, "password:")
        || tail_ends_with_ci(trimmed, "passphrase:")
        || tail_ends_with_ci(trimmed, "new password:")
        || tail_ends_with_ci(trimmed, "current password:");
}

static void ssh_watch_for_password_prompt(Runtime *conn, unsigned char byte)
{
    if (conn->ssh_waiting_for_password)
        return;

    if (byte == '\n' || byte == '\r') {
        if (conn->never_echo_heuristic) {
            conn->never_echo = false;
            conn->never_echo_heuristic = false;
        }
        conn->ssh_output_tail.clear();
        return;
    }

    if (byte < 0x20 || byte >= 0x7f)
        return;

    conn->ssh_output_tail.push_back((char)byte);
    if (conn->ssh_output_tail.size() > kSshTailMax)
        conn->ssh_output_tail.erase(0, conn->ssh_output_tail.size() - kSshTailMax);

    if (!conn->never_echo && looks_like_password_prompt(conn->ssh_output_tail)) {
        conn->never_echo = true;
        conn->never_echo_heuristic = true;
        conn->never_echo_deadline =
            std::chrono::steady_clock::now() + kNeverEchoTimeout;
    }
}

void ssh_check_never_echo_timeout(Runtime *conn)
{
    if (conn->never_echo_heuristic &&
        std::chrono::steady_clock::now() > conn->never_echo_deadline) {
        conn->never_echo = false;
        conn->never_echo_heuristic = false;
    }
}

// ---------------------------------------------------------------------------
// libssh2 one-time init
// ---------------------------------------------------------------------------

static void libssh2_global_init()
{
    static std::once_flag flag;
    std::call_once(flag, []{ libssh2_init(0); });
}

// ---------------------------------------------------------------------------
// EAGAIN retry helper
//
// Calls op(); if it returns LIBSSH2_ERROR_EAGAIN, waits on the socket for
// the direction libssh2 needs, then calls itself again. On success (rc >= 0)
// calls on_done. On other errors calls on_fail(rc).
// ---------------------------------------------------------------------------

static void ssh_retry(std::shared_ptr<Runtime> self,
                      std::function<int()> op,
                      std::function<void()> on_done,
                      std::function<void(int)> on_fail)
{
    int rc = op();
    if (rc == LIBSSH2_ERROR_EAGAIN) {
        int dir = libssh2_session_block_directions(self->ssh_session_);
        auto wt = (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
                  ? tcp::socket::wait_write
                  : tcp::socket::wait_read;
        self->socket_->async_wait(wt, [=](asio::error_code ec) {
            if (!ec) ssh_retry(self, op, on_done, on_fail);
            else     on_fail(-1);
        });
        return;
    }
    if (rc >= 0) on_done();
    else         on_fail(rc);
}

// ---------------------------------------------------------------------------
// Host key verification (accept-new policy using ~/.ssh/known_hosts)
// ---------------------------------------------------------------------------

static std::string known_hosts_path()
{
    const char *home = getenv("HOME");
    if (!home) home = "/root";
    return std::string(home) + "/.ssh/known_hosts";
}

static bool ssh_verify_hostkey(Runtime *self)
{
    size_t keylen;
    int keytype;
    const char *key = libssh2_session_hostkey(self->ssh_session_, &keylen, &keytype);
    if (!key) {
        self->grid->info(_("/// SSH: could not retrieve host key\n"));
        return false;
    }

    LIBSSH2_KNOWNHOSTS *kh = libssh2_knownhost_init(self->ssh_session_);
    if (!kh) return false;

    std::string kh_path = known_hosts_path();
    libssh2_knownhost_readfile(kh, kh_path.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    struct libssh2_knownhost *found = nullptr;
    int check = libssh2_knownhost_checkp(kh,
        self->host.c_str(), self->port,
        key, keylen,
        LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW,
        &found);

    if (check == LIBSSH2_KNOWNHOST_CHECK_MATCH) {
        libssh2_knownhost_free(kh);
        return true;
    }

    if (check == LIBSSH2_KNOWNHOST_CHECK_NOTFOUND) {
        // First connection to this host: add the key and proceed (accept-new policy,
        // equivalent to OpenSSH StrictHostKeyChecking=accept-new).
        int kh_keytype;
        switch (keytype) {
            case LIBSSH2_HOSTKEY_TYPE_RSA: kh_keytype = LIBSSH2_KNOWNHOST_KEY_SSHRSA;  break;
            case LIBSSH2_HOSTKEY_TYPE_DSS: kh_keytype = LIBSSH2_KNOWNHOST_KEY_SSHDSS;  break;
#ifdef LIBSSH2_KNOWNHOST_KEY_ECDSA_256
            case LIBSSH2_HOSTKEY_TYPE_ECDSA_256: kh_keytype = LIBSSH2_KNOWNHOST_KEY_ECDSA_256; break;
            case LIBSSH2_HOSTKEY_TYPE_ECDSA_384: kh_keytype = LIBSSH2_KNOWNHOST_KEY_ECDSA_384; break;
            case LIBSSH2_HOSTKEY_TYPE_ECDSA_521: kh_keytype = LIBSSH2_KNOWNHOST_KEY_ECDSA_521; break;
#endif
#ifdef LIBSSH2_KNOWNHOST_KEY_ED25519
            case LIBSSH2_HOSTKEY_TYPE_ED25519:   kh_keytype = LIBSSH2_KNOWNHOST_KEY_ED25519;   break;
#endif
            default: kh_keytype = LIBSSH2_KNOWNHOST_KEY_SSHRSA; break;
        }
        libssh2_knownhost_addc(kh,
            self->host.c_str(), nullptr,
            key, keylen,
            nullptr, 0,
            LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW | kh_keytype,
            nullptr);
        libssh2_knownhost_writefile(kh, kh_path.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH);
        libssh2_knownhost_free(kh);
        self->grid->infof(_("/// SSH: added host key for {} to known_hosts\n"), self->host);
        return true;
    }

    // MISMATCH or FAILURE
    libssh2_knownhost_free(kh);
    self->grid->infof(_("/// SSH: WARNING: host key mismatch for {} — connection aborted\n"), self->host);
    return false;
}

// ---------------------------------------------------------------------------
// Forward declarations for auth continuation
// ---------------------------------------------------------------------------

static void ssh_open_channel(std::shared_ptr<Runtime> self);
static void ssh_try_pubkey(std::shared_ptr<Runtime> self);
static void ssh_try_password(std::shared_ptr<Runtime> self);

// ---------------------------------------------------------------------------
// Channel setup (open session -> request PTY -> start shell -> connected)
// ---------------------------------------------------------------------------

static void ssh_start_shell(std::shared_ptr<Runtime> self)
{
    auto op = [self]() -> int {
        return libssh2_channel_shell(self->ssh_channel_);
    };
    auto done = [self]() {
        self->on_ssh_connected();
    };
    auto fail = [self](int rc) {
        self->grid->infof(_("/// SSH: could not start shell ({})\n"), rc);
        self->disconnected(0, 0);
    };
    ssh_retry(self, op, done, fail);
}

static void ssh_request_pty(std::shared_ptr<Runtime> self)
{
    auto op = [self]() -> int {
        return libssh2_channel_request_pty(self->ssh_channel_, "xterm-256color");
    };
    auto done = [self]() { ssh_start_shell(self); };
    auto fail = [self](int rc) {
        self->grid->infof(_("/// SSH: could not request PTY ({})\n"), rc);
        self->disconnected(0, 0);
    };
    ssh_retry(self, op, done, fail);
}

static void ssh_open_channel(std::shared_ptr<Runtime> self)
{
    auto op = [self]() -> int {
        self->ssh_channel_ = libssh2_channel_open_session(self->ssh_session_);
        if (self->ssh_channel_) return 0;
        return libssh2_session_last_errno(self->ssh_session_);
    };
    auto done = [self]() { ssh_request_pty(self); };
    auto fail = [self](int rc) {
        self->grid->infof(_("/// SSH: could not open channel ({})\n"), rc);
        self->disconnected(0, 0);
    };
    ssh_retry(self, op, done, fail);
}

// ---------------------------------------------------------------------------
// Authentication
// ---------------------------------------------------------------------------

static std::vector<std::string> candidate_keys(const std::string &override_path)
{
    std::vector<std::string> keys;
    if (!override_path.empty()) {
        keys.push_back(override_path);
        return keys;
    }
    const char *home = getenv("HOME");
    if (!home) return keys;
    for (const char *name : {"id_ed25519", "id_ecdsa", "id_rsa"}) {
        keys.push_back(std::string(home) + "/.ssh/" + name);
    }
    return keys;
}

void ssh_continue_auth(std::shared_ptr<Runtime> self)
{
    ssh_try_password(self);
}

static void ssh_try_password(std::shared_ptr<Runtime> self)
{
    if (self->ssh_password.empty()) {
        for (const char *p = _("Password: "); *p; ++p)
            self->grid->place(Cell((wchar_t)(unsigned char)*p));
        self->grid->changed = true;
        self->never_echo = true;
        self->ssh_waiting_for_password = true;
        tty.bad_have = true;
        self->display_buffer();
        fflush(stdout);
        return;
    }

    std::string user = self->ssh_username;
    std::string pass = self->ssh_password;

    auto op = [self, user, pass]() -> int {
        return libssh2_userauth_password(self->ssh_session_, user.c_str(), pass.c_str());
    };
    auto done = [self]() { ssh_open_channel(self); };
    auto fail = [self](int) {
        self->grid->info(_("/// SSH: authentication failed\n"));
        self->disconnected(0, 0);
    };
    ssh_retry(self, op, done, fail);
}

static void ssh_try_pubkey_at(std::shared_ptr<Runtime> self,
                              std::vector<std::string> keys, size_t idx)
{
    if (idx >= keys.size()) {
        // No key worked — fall back to password
        ssh_try_password(self);
        return;
    }

    std::string priv = keys[idx];
    std::string pub = priv + ".pub";
    std::string user = self->ssh_username;

    auto op = [self, user, pub, priv]() -> int {
        return libssh2_userauth_publickey_fromfile(
            self->ssh_session_,
            user.c_str(),
            pub.c_str(),
            priv.c_str(),
            nullptr);
    };
    auto done = [self]() { ssh_open_channel(self); };
    auto fail = [self, keys, idx](int) {
        // try next key
        ssh_try_pubkey_at(self, keys, idx + 1);
    };
    ssh_retry(self, op, done, fail);
}

static void ssh_try_pubkey(std::shared_ptr<Runtime> self)
{
    auto keys = candidate_keys(self->ssh_key_path);
    ssh_try_pubkey_at(self, keys, 0);
}

// ---------------------------------------------------------------------------
// Handshake entry point
// ---------------------------------------------------------------------------

void ssh_begin_handshake(std::shared_ptr<Runtime> self)
{
    libssh2_global_init();

    self->ssh_session_ = libssh2_session_init();
    if (!self->ssh_session_) {
        self->grid->info(_("/// SSH: could not create session\n"));
        self->disconnected(0, 0);
        return;
    }

    libssh2_session_set_blocking(self->ssh_session_, 0);

    int sockfd = (int)self->socket_->native_handle();

    auto op = [self, sockfd]() -> int {
        return libssh2_session_handshake(self->ssh_session_, sockfd);
    };
    auto done = [self]() {
        if (!ssh_verify_hostkey(self.get())) {
            self->disconnected(0, 0);
            return;
        }
        ssh_try_pubkey(self);
    };
    auto fail = [self](int rc) {
        self->grid->infof(_("/// SSH: handshake failed ({})\n"), rc);
        self->disconnected(0, 0);
    };
    ssh_retry(self, op, done, fail);
}

// ---------------------------------------------------------------------------
// Read loop
// ---------------------------------------------------------------------------

void ssh_do_read(std::shared_ptr<Runtime> self)
{
    self->socket_->async_wait(tcp::socket::wait_read,
        [self](asio::error_code ec) {
            if (ec) {
                self->fail("ssh read", ec);
                return;
            }

            char buf[4096];
            ssize_t n;
            while ((n = libssh2_channel_read(self->ssh_channel_, buf, sizeof(buf))) > 0) {
                for (ssize_t i = 0; i < n; i++) {
                    unsigned char b = (unsigned char)buf[i];
                    decode(self.get(), self->cur_grid, b);
                    ssh_watch_for_password_prompt(self.get(), b);
                }
            }

            if (libssh2_channel_eof(self->ssh_channel_)) {
                self->disconnected(0, 0);
                return;
            }

            if (self->grid->changed) {
                self->display_buffer();
                fflush(stdout);
            }

            ssh_do_read(self);
        });
}

// ---------------------------------------------------------------------------
// Write
// ---------------------------------------------------------------------------

static void ssh_write_at(std::shared_ptr<Runtime> self, std::string data, size_t off)
{
    ssize_t w = libssh2_channel_write(
        self->ssh_channel_,
        data.data() + off,
        data.size() - off);

    if (w == LIBSSH2_ERROR_EAGAIN) {
        int dir = libssh2_session_block_directions(self->ssh_session_);
        auto wt = (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
                  ? tcp::socket::wait_write
                  : tcp::socket::wait_read;
        self->socket_->async_wait(wt, [self, data, off](asio::error_code ec) {
            if (!ec) ssh_write_at(self, data, off);
        });
        return;
    }
    if (w < 0) return;
    off += (size_t)w;
    if (off < data.size())
        ssh_write_at(self, data, off);
}

void ssh_write(std::shared_ptr<Runtime> self, const std::string &data)
{
    ssh_write_at(self, data, 0);
}

// ---------------------------------------------------------------------------
// Teardown
// ---------------------------------------------------------------------------

void ssh_disconnect(Runtime *self)
{
    if (self->ssh_channel_) {
        libssh2_channel_close(self->ssh_channel_);
        libssh2_channel_free(self->ssh_channel_);
        self->ssh_channel_ = nullptr;
    }
    if (self->ssh_session_) {
        libssh2_session_disconnect(self->ssh_session_, "Normal shutdown");
        libssh2_session_free(self->ssh_session_);
        self->ssh_session_ = nullptr;
    }
}

#endif // HAVE_LIBSSH2
