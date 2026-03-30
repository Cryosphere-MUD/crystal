#pragma once

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <iostream>
#include <memory>
#include <term.h>

#undef lines
#undef newline
#undef grid

#include "crystal.h"
#include "grid.h"
#include "telnet.h"
#include "esc.h"

extern mterm tty;

using asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
        
    std::array<char, 4096> stdin_raw_;
    std::array<char, 4096> socket_raw_;
    grid_t grid;
    conn_t *conn;

public:
    Session(asio::io_context& io, bool use_ssl)
        : resolver_(io),
          socket_(io),
          ssl_ctx_(asio::ssl::context::tls_client),
          ssl_stream_(socket_, ssl_ctx_),
          stdin_(io, ::dup(STDIN_FILENO)),
          use_ssl_(use_ssl) {

        conn = new conn_t(&grid);
        conn->commandmode = true;
     	conn->initbindings();

        scripting::set_grid(&grid);
	    scripting::start();

        if (use_ssl_) {
            ssl_ctx_.set_default_verify_paths();
            ssl_stream_.set_verify_mode(asio::ssl::verify_none);
        }
    }

    void start(const std::string& host, const std::string& port) {
        host_ = host;
        port_ = port;

        // std::cerr << __PRETTY_FUNCTION__ << " " << host_ << std::endl;
        // std::cerr << "sending to grid " << conn->grid << std::endl;
        conn->grid->infof("/// Crystal\n");
        conn->grid->infof("/// resolving %s\n", host.c_str());
        conn->grid->changed = true;
        conn->display_buffer();
        conn->commandmode = false;

        resolver_.async_resolve(host, port,
            [self = shared_from_this()](auto ec, auto results) {

                if (ec) return self->fail("resolve", ec);

                asio::async_connect(self->socket_, results,
                    [self](auto ec, auto) {
                        if (ec) return self->fail("connect", ec);

                        if (self->use_ssl_) {
                            self->ssl_stream_.async_handshake(
                                asio::ssl::stream_base::client,
                                [self](auto ec) {
                                    if (ec) return self->fail("handshake", ec);
                                    self->on_connected();
                                });
                        } else {
                            self->on_connected();
                        }
                    });
            });
    }

private:
    void on_connected() {
        // std::cout << "Connected to " << host_ << ":" << port_ << "\n";
        conn->connected();

        conn->telnet = std::make_shared<telnet_state>(socket_);
    
        do_read_socket();
    }

    void do_read_socket() {
        auto self = shared_from_this();

        // std::cerr << __PRETTY_FUNCTION__ << std::endl;

        // auto handler = 

        socket_.async_read_some(asio::buffer(socket_raw_), [self](auto ec, std::size_t n) {
            if (ec) return self->fail("read", ec);

            std::string data(self->socket_raw_.data(), n);

            const char *data2 = data.data();

            self->conn->telnet->handle_read(self->conn, (unsigned char*)data2, data.size());

            // std::istream is(&self->socket_buf_);
            // std::string line;
            // std::getline(is, line);

            // std::cout << "[remote] " << line << "\n";

            if (self->conn->grid->changed)
                self->conn->display_buffer();

            self->do_read_socket();
        });

        // if (use_ssl_) {
        //     asio::async_read_some(asio::buffer()self._, ssl_stream_, socket_buf_, '\n', handler);
        // } else {
        //     asio::async_read_some(socket_, socket_buf_, '\n', handler);
        // }
    }

    public:
void do_read_stdin() {
    // std::cerr << __PRETTY_FUNCTION__ << std::endl;

    auto self = shared_from_this();

    self->grid.changed = true;

    self->conn->display_buffer();

    stdin_.async_read_some(asio::buffer(stdin_raw_),
        [self](auto ec, std::size_t n) {
            if (ec) return self->fail("stdin", ec);

            std::string data(self->stdin_raw_.data(), n);

            auto conn = self->conn;

            // std::cerr << "feeding " << esc(data) << std::endl;

				tty.feed(data);

                // std::cerr << "feed " << esc(data) << std::endl;

				for (wchar_t i : tty.decode_feed())
				{
					if (conn->telnet && conn->telnet->charmode && !conn->commandmode && i != 0x1d)
					{
						std::string p;
						p += i;
						conn->telnet->send(p);
					}
					else
					{
						my_wstring s = tty.convert_input(i);
						if (s.length())
                        {
                            // std::cerr << "dispatching key " << esc(s) << std::endl;
							conn->dispatch_key(s);
                        }
					}

					if (!conn->telnet)
						conn->commandmode = 1;

                    // std::cerr << "buffer " << esc(conn->buffer) << " cursor " << conn->cursor << std::endl;

					conn->grid->changed = true;
				}
				// else
				// {
				// 	break;
				// }

            // self->do_write(data);  // send immediately

            self->grid.changed = true;

            self->conn->display_buffer();
			fflush(stdout);


            if (conn->quit)
                exit(0);
            else
                self->do_read_stdin(); // continue reading

            });
    }

    void do_write(const std::string& msg) {
        auto self = shared_from_this();

        auto handler = [self](auto ec, std::size_t) {
            if (ec) return self->fail("write", ec);
        };

        if (use_ssl_) {
            asio::async_write(ssl_stream_, asio::buffer(msg), handler);
        } else {
            asio::async_write(socket_, asio::buffer(msg), handler);
        }
    }

    void fail(const std::string& what, asio::error_code ec) {
        std::cout << what << " error: " << ec.message() << "\n";

        asio::error_code ignored;
        socket_.close(ignored);
    }

private:
    tcp::resolver resolver_;
    tcp::socket socket_;

    asio::ssl::context ssl_ctx_;
    asio::ssl::stream<tcp::socket&> ssl_stream_;

    asio::posix::stream_descriptor stdin_;

    asio::streambuf socket_buf_;
    asio::streambuf stdin_buf_;

    std::string host_, port_;
    bool use_ssl_;
};
