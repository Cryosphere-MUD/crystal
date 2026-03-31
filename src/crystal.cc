/*
 * Crystal Mud Client
 * Copyright (C) 2002-2010 Abigail Brady
 * Copyright (C) 2002 Owen Cliffe
 * Copyright (C) 2004 Stuart Brady
 * Copyright (C) 2004 Paul Lettington
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

/*
 *
 *   stdin -> charset filter -> esc seq interpreter -> keycode interpreter -> 
 *                            cmd interpreter
 *
 *   read or sslread -> telnet decoder -> charset filter -> tty handler
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <asio.hpp>

#include <iostream>

#include <curses.h>
#include <term.h>

#include <langinfo.h>
#include <locale.h>
#include <wchar.h>

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <netinet/in.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include <iconv.h>

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <ctype.h>
#include <stdarg.h>

#undef lines
#undef newline
#undef grid

#include "Socket.h"
#include "url.h"

#undef SCROLL

//FILE *logfile = 0;

class conn_t;
class grid_t;

#include "commandeditor.h"
#include "commands.h"
#include "crystal.h"
#include "grid.h"
#include "io.h"
#include "scripting.h"
#include "telnet.h"

mterm tty;

extern "C" int mk_wcwidth(wchar_t ucs);

size_t real_wcwidth(wchar_t u)
{
	int wid = wcwidth(u);
	if (wid > 0)
		return wid;
	wid = mk_wcwidth(u);
	if (wid > 0)
		return wid;
	wid = 1;
	return 1;
}

const cell_t blank(0);

conn_t::conn_t(asio::io_context& io, grid_t *gr) 
: resolver_(io),
          socket_(io),
          ssl_ctx_(asio::ssl::context::tls_client),
          ssl_stream_(socket_, ssl_ctx_),
          stdin_(io, ::dup(STDIN_FILENO))
        //   use_ssl_(false)
{
	cur_grid = grid = gr;
	overlay = new grid_t();

	overlay->defbc = 0;
	overlay->backcol = 0;
	overlay->visible = false;

	gr->set_conn(this);
	if (overlay)
		overlay->set_conn(this);
}

conn_t::~conn_t()
{
	if (overlay)
		delete overlay;
}

void conn_t::dofindnext()
{
	for (int i = hardscroll; i < grid->row; i++)
	{
		my_wstring s;
		for (int j = 0; j < grid->get_len(i); j++)
			s += grid->get(i, j).ch;

		std::set<my_wstring>::iterator it;
		for (it = hl_matches.begin(); it != hl_matches.end(); it++)
		{
			size_t l = s.find(*it);
			while (l != std::string::npos)
			{
				for (size_t j = 0; j < it->length(); j++)
				{
					hardscroll = i + 1;
					if (hardscroll > (grid->row - tty.HEIGHT))
						hardscroll = 0;
					grid->changed = true;
					return;
				}
				l = s.find(*it, l + 1);
			}
		}
	}
	hardscroll = 0;
	grid->changed = true;
}

void conn_t::dotoggleoverlay()
{
	if (overlay)
	{
		overlay->visible = !overlay->visible;
		overlay->changed = true;
	}
}

void conn_t::triggerfn(const char *fn)
{
	std::string st = "fn.";
	st += fn;
	st += "\r\n";

	if (telnet)
		telnet->send(st);
}

void conn_t::doscrollstart()
{
	if (grid->row < tty.HEIGHT)
		return;
	hardscroll = 1;
	grid->changed = true;
}

void conn_t::doscrollend()
{
	hardscroll = 0;
	grid->changed = true;
}

void conn_t::dorefresh()
{
	grid->changed = true;
	tty.bad_have = true;
}

void conn_t::dosuspend()
{
	kill(0, SIGTSTP);

	struct termios ti;
	cfmakeraw(&ti);
	tcsetattr(0, TCSADRAIN, &ti);

	tty.grabwinsize();

	tty.bad_have = true;
	grid->changed = true;
	tty.show_want();
	if (tty.titleset)
	{
		tty.titleset = 0;
		tty.title("%s", tty.curtitle.c_str());
	}
}

void conn_t::doenter()
{
	conn_t *conn = this;

	if (in_commandmode())
	{
		my_wstring s = conn->buffer;
		conn->doclearline();

		if (s.length() == 0)
		{
			set_commandmode(false);
			return;
		}

		chist()->insert(s);

		set_commandmode(false);

		conn->grid->info(_("crystal> "));
		conn->grid->info(s);
		conn->grid->info("\n");

		docommand(conn, s);

		return;
	}

	my_wstring wproper = L"";
	std::string proper = "";

	for (wchar_t sb : conn->buffer)
	{
		wproper += sb;

		static iconv_t i = 0;
		static std::string cur_cset = "";

		if (!i || cur_cset != conn->mud_cset)
		{
			if (i)
				iconv_close(i);
			i = iconv_open(conn->mud_cset.c_str(), "WCHAR_T");
			cur_cset = conn->mud_cset;
		}
		wchar_t ic = sb;
		iconv_inptr_t ibuf = (iconv_inptr_t)&ic;
		size_t ilen = 4;
		char o[11];
		char *obuf = o;
		size_t olen = 10;
		iconv(i, &ibuf, &ilen, &obuf, &olen);
		if (olen != 10)
		{
			*obuf = 0;
			proper += o;
		}
	}

	// If there was stored prompt, echo this into the buffer proper and erase it.
	if (conn->grid->cstoredprompt.length())
	{
		for (int i = 0; i < conn->grid->cstoredprompt.length(); i++)
			conn->grid->place(&conn->grid->cstoredprompt[i]);
		conn->grid->cstoredprompt.erase();
	}

	conn->grid->lastprompt = false;

	bool toecho = true, tohistory = true;

	if (conn->never_echo)
		toecho = false;

	if (conn->telnet && conn->telnet->will_echo)
		tohistory = toecho = false;

	if (tohistory && wproper.length())
	{
		// put the commandline in the history if it isn't a password
		chist()->insert(wproper);
	}
	nofuture = true;

	if (toecho)
	{
		// echo the commandline if appropriate
		my_wstring a;
		for (int i = 0; i < conn->grid->col; i++)
			a += conn->grid->get(conn->grid->row, i).ch;
		for (size_t i = 0; i < wproper.length(); i++)
		{
			cell_t c = cell_t(wproper[i]);
			conn->grid->place(&c);
		}
		conn->grid->wantnewline();
		conn->grid->changed = true;
	}

	proper += "\r\n";

	// send it
	if (conn->telnet)
		conn->telnet->send(proper);

	conn->doclearline();
}

void conn_t::doscrolldown()
{
	if (hardscroll)
		hardscroll += 10;
	if (hardscroll > (grid->row - tty.HEIGHT))
		hardscroll = 0;
	grid->changed = true;
}

void conn_t::doscrollup()
{
	if (grid->row < tty.HEIGHT)
		return;

	if (hardscroll)
		hardscroll -= 10;
	else
		hardscroll = (grid->row - tty.HEIGHT) - 9;
	if (hardscroll < 1)
		hardscroll = 1;
	grid->changed = true;
}

void conn_t::docommandmode()
{
	if (!in_commandmode())
		doclearline();
	set_commandmode(true);
}

void conn_t::connected()
{
	conn_t *conn = this;

        // std::cerr << "sending to grid " << conn->grid << std::endl;

	conn->grid->infof(_("/// connected with %s\n"), "telnet");

	static int printed_escape_line = 0;
	if (!printed_escape_line)
	{
		conn->grid->infof(_("/// escape character is '%s'\n"), "^]");
		printed_escape_line = 1;
	}

	if (conn->ssl)
		tty.title(_("telnets://%s:%i - Crystal"), conn->host.c_str(), conn->port);
	else
		tty.title(_("telnet://%s:%i - Crystal"), conn->host.c_str(), conn->port);

    	display_buffer();
}

bool conn_t::file_log(const char *filename)
{
	if (logfile)
	{
		grid->info(_("/// closing old logfile.\n"));
		fclose(logfile);
		logfile = NULL;
	}
	logfile = fopen(filename, "a");
	if (!logfile)
	{
		grid->infof(_("/// couldn't open '%s' for appending.\n"), filename);
		return false;
	}
	else
	{
		grid->infof(_("/// logging to end of '%s'.\n"), filename);
		return true;
	}
}

bool conn_t::disconnected(int bts, int pend)
{
	conn_t *conn = this;
	tty.title(_("Disconnected - Crystal"));
	if (conn->grid->col)
		conn->grid->newline();
#if 0
	if (bts == -1)
	{
		int e = errno;
		conn->grid->infof(_("/// connection closed : %s.\n"), strerror(e));
		if (pend && (conn->addr_i < conn->addrs->size()))
		{
			conn->addr_i++;
			if (conn->try_addr(conn->host.c_str(), conn->port, conn->ssl))
			{
				conn->display_buffer();
				conn->grid->changed = true;
				fflush(stdout);
				return true;
			}
		}
	}
	else
	{
#endif
		conn->grid->info(_("/// connection closed by foreign host.\n"));
#if 0
	}
#endif
	conn->display_buffer();
	fflush(stdout);
	conn->telnet.reset();
	conn->set_commandmode(true);
	conn->grid->changed = true;
	return false;
}

void do_read(conn_t *conn,
	     asio::posix::stream_descriptor& stream_desc, 
             std::array<char, 256>& buffer);

void handle_input(conn_t* conn,
		  const asio::error_code& error, 
                  size_t bytes_transferred, 
                  asio::posix::stream_descriptor& stream_desc, 
                  std::array<char, 256>& buffer) {
    if (!error) {
        for (size_t idx = 0; idx < bytes_transferred; idx++) {
            my_wstring s = tty.convert_input(buffer[idx]);
            if (s.length()) {
                conn->dispatch_key(s);
            }
		if (!conn->telnet)
			conn->set_commandmode(true);
		conn->grid->changed = 1;
        }
        
	if (!conn->quit)
	{
	        do_read(conn, stream_desc, buffer);
		conn->display_buffer();
		fflush(stdout);
	}
	else
	{
		exit(0);
	}

    }
}

void do_read(conn_t * conn,
	     asio::posix::stream_descriptor& stream_desc, 
             std::array<char, 256>& buffer) {
   stream_desc.async_read_some(asio::buffer(buffer), 
        [conn, &stream_desc, &buffer](const asio::error_code& error, size_t bytes_transferred) {
            handle_input(conn, error, bytes_transferred, stream_desc, buffer);
        });
}


void conn_t::main_loop(asio::io_context &io_context)
{
	conn_t *conn = this;

	// if (!conn->telnet)
	// 	conn->set_commandmode(true);

	conn->grid->changed = true;
	tty.bad_have = true;

	conn->display_buffer();

	asio::posix::stream_descriptor stdin_desc(io_context, STDIN_FILENO);

	std::array<char, 256> input_buffer;

	assert(!cursor);

	display_buffer();
	fflush(stdout);

	do_read(this, stdin_desc, input_buffer);

	io_context.run();

	stdin_desc.release();
}

void conn_t::connect(const std::string& host, const std::string& port, bool ssl) {
        // host_ = host;
        // port_ = port;

	this->ssl = ssl;

        grid->infof("/// resolving %s\n", host.c_str());
        grid->changed = true;
        set_commandmode(false);
        display_buffer();

	// std::cerr << "going to resolve " << std::endl;

        resolver_.async_resolve(host, port,
            [self = shared_from_this()](asio::error_code ec, auto results) {

                if (ec) return self->fail("resolve", ec);

		// std::cerr << "resolved happened " << std::endl;

        for (auto const& entry : results) {
            auto endpoint = entry.endpoint();
            std::string ip = endpoint.address().to_string(); // e.g., "93.184.216.34"
            unsigned short port = endpoint.port();           // e.g., 80
            self->grid->infof("/// connecting to %s:%i\n", ip.c_str(), port);
        }

	        self->grid->changed = true;
	        self->display_buffer();

                asio::async_connect(self->socket_, results,
                    [self](auto ec, auto) {

			std::cerr << "connect happened? " << std::endl;

                        if (ec) return self->fail("connect", ec);

                        if (self->ssl) {
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


	   void conn_t::on_connected() {
        // std::cout << "Connected to " << host_ << ":" << port_ << "\n";
        connected();

        telnet = std::make_shared<telnet_state>(socket_);

        do_read_socket();
    }

    void conn_t::do_read_socket() {
        auto self = shared_from_this();

        // std::cerr << __PRETTY_FUNCTION__ << std::endl;

        auto handler = [self](auto ec, std::size_t n) {
            if (ec) return self->fail("read", ec);

            std::string data(self->socket_raw_.data(), n);

            const char *data2 = data.data();

            self->telnet->handle_read(self.get(), (unsigned char*)data2, data.size());

            // std::istream is(&self->socket_buf_);
            // std::string line;
            // std::getline(is, line);

            // std::cout << "[remote] " << line << "\n";

            if (self->grid->changed)
                self->display_buffer();

            self->do_read_socket();
        };

	if (ssl)
	        ssl_stream_.async_read_some(asio::buffer(socket_raw_), handler);
	else
	        socket_.async_read_some(asio::buffer(socket_raw_), handler);
}

void conn_t::fail(const std::string& what, asio::error_code ec) {
        	telnet.reset();
		set_commandmode(true);

        	asio::error_code ignored;
        	socket_.close(ignored);
		display_buffer();
    	}

void conn_t::set_commandmode(bool new_command_mode)
{
	if (in_commandmode() == new_command_mode)
		return;

	// grid->infof("///set_commandmode called with %i\n", new_command_mode);
	// display_buffer();

	commandeditor_t::set_commandmode(new_command_mode);
}

    struct termios oldti;
