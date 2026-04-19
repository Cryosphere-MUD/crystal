/* 
 * Crystal Mud Client
 * Copyright (C) Abigail Brady, Paul Lettington, Owen Cliffe, Stuart Brady
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */

#ifndef RUNTIME_H
#define RUNTIME_H

#include <asio.hpp>
#include <asio/ssl.hpp>

#ifdef HAVE_LIBSSH2
#include <libssh2.h>
#endif

#include <memory>
#include <set>
#include <string>

#include "commandeditor.h"
#include "common.h"

using asio::ip::tcp;

struct TelnetState;
class ANSIGrid;

class CommandHistory;

class Runtime : public CommandEditor, public std::enable_shared_from_this<Runtime>
{
      private:
	//! the amount we have scrolled to in the buffer
	int hardscroll = 0;

      public:
	//! are we in never echo mode (default: no)
	bool never_echo = false;

	//! are we in kludge lp prompts mode (default: yes)
	bool lp_prompts = true;

	//! are we quitting?
	bool quit = false;

	ANSIGrid *grid = nullptr;

	ANSIGrid *overlay = nullptr;
	ANSIGrid *cur_grid = nullptr;

	std::shared_ptr<TelnetState> telnet = nullptr;
	FILE *logfile = nullptr;

	enum class ConnectionType { Telnet, TelnetSSL, SSH };

	std::string host;
	int port = 0;
	ConnectionType conn_type = ConnectionType::Telnet;

	std::string mud_cset = "ISO-8859-1";

#ifdef HAVE_LIBSSH2
	std::string ssh_username;
	std::string ssh_password;
	std::string ssh_key_path;
	LIBSSH2_SESSION *ssh_session_ = nullptr;
	LIBSSH2_CHANNEL *ssh_channel_ = nullptr;
	bool ssh_waiting_for_password = false;
#endif
	std::string ssh_default_key_path;

	std::set<String32> hl_matches;

	std::array<char, 4096> stdin_raw_;
	std::array<char, 4096> socket_raw_;

	tcp::resolver resolver_;

	asio::io_context &io_;

	asio::ssl::context ssl_ctx_;

	bool reconnecting = false;

	std::unique_ptr<tcp::socket> socket_;
	std::unique_ptr<asio::ssl::stream<tcp::socket &>> ssl_stream_;
	// asio::ssl::stream<tcp::socket&> ssl_stream_;

	asio::posix::stream_descriptor stdin_;

	asio::streambuf socket_buf_;
	asio::streambuf stdin_buf_;

	Runtime(asio::io_context &io, ANSIGrid *grid);
	~Runtime();

	void dorefresh();

	void doscrollup();
	void doscrolldown();
	void doscrollstart();
	void doscrollend();
	void triggerfn(const std::string &which);
	void dofindnext();

	void doenter();
	void dosuspend();
	void dotoggleoverlay();
	void docommandmode();

	void show_lines_at(int from, int to, int num);

	void initbindings();
	void dispatch_key(const String32 &s);
	void addbinding(const wchar_t *key, const std::string &bind);

	void connect(const std::string &host, const std::string &port, ConnectionType type,
	             const std::string &username = "",
	             const std::string &password = "",
	             const std::string &key_path = "");
	void send_to_server(const std::string &data);
	void on_ssh_connected();
	bool file_log(const std::string &filename);
	void do_read_from_socket();

	void display_buffer();

	void queue_repaint();

	bool disconnected(int bts, int pend);
	void connected();

	void main_loop(asio::io_context &io_context);

	void fail(const std::string &what, asio::error_code ec);

	void start(const std::string &host, const std::string &port);

	void on_connected();

	void set_commandmode(bool new_command_mode) override;

	void do_read_socket();
};

extern int exitValue;

#endif
