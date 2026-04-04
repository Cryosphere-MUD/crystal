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

#include "url.h"

#undef SCROLL

//FILE *logfile = 0;

class Runtime;
class ANSIGrid;

#include <widecharwidth/widechar_width.h>

#include "commandeditor.h"
#include "commands.h"
#include "crystal.h"
#include "grid.h"
#include "io.h"
#include "scripting.h"
#include "telnet.h"

Output tty;

int real_wcwidth(char32_t ch)
{
        int width = widechar_wcwidth(ch);
        switch (width)
        {
        case widechar_nonprint:
                return 0;
        case widechar_combining:
                return 0;
        case widechar_ambiguous:
                return 1;
        case widechar_private_use:
                return 1;
        case widechar_unassigned:
                return 1;
        case widechar_widened_in_9:
                return 2;
        case widechar_non_character:
                return 0;
        }
        return width;
}

const Cell blank(0);

Runtime::Runtime(asio::io_context &io, ANSIGrid *gr)
    : io_(io), resolver_(io), ssl_ctx_(asio::ssl::context::tls_client), stdin_(io, ::dup(STDIN_FILENO))
//   use_ssl_(false)
{
	ssl_ctx_.set_verify_mode(asio::ssl::verify_peer);
	ssl_ctx_.set_default_verify_paths();  // use system CA certificates

	cur_grid = grid = gr;
	overlay = new ANSIGrid();

	overlay->defbc = 0;
	overlay->backcol = 0;
	overlay->visible = false;

	gr->set_conn(this);
	if (overlay)
		overlay->set_conn(this);
}

Runtime::~Runtime()
{
	if (overlay)
		delete overlay;
}

void Runtime::dofindnext()
{
	for (int i = hardscroll; i < grid->row; i++)
	{
		String32 s;
		for (int j = 0; j < grid->get_len(i); j++)
			s += grid->get(i, j).ch;

		std::set<String32>::iterator it;
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

void Runtime::dotoggleoverlay()
{
	if (overlay)
	{
		overlay->visible = !overlay->visible;
		overlay->changed = true;
	}
}

void Runtime::triggerfn(const std::string &fn)
{
	std::string st = "fn.";
	st += fn;
	st += "\r\n";

	if (telnet)
		telnet->send(st);
}

void Runtime::doscrollstart()
{
	if (grid->row < tty.HEIGHT)
		return;
	hardscroll = 1;
	grid->changed = true;
}

void Runtime::doscrollend()
{
	hardscroll = 0;
	grid->changed = true;
}

void Runtime::dorefresh()
{
	grid->changed = true;
	tty.bad_have = true;
}

void Runtime::dosuspend()
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
		tty.title(tty.curtitle);
	}
}

void Runtime::doenter()
{
	Runtime *conn = this;

	if (in_commandmode())
	{
		String32 s = conn->buffer;
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

	String32 wproper = L"";
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
		String32 a;
		for (int i = 0; i < conn->grid->col; i++)
			a += conn->grid->get(conn->grid->row, i).ch;
		for (size_t i = 0; i < wproper.length(); i++)
		{
			Cell c = Cell(wproper[i]);
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

void Runtime::doscrolldown()
{
	if (hardscroll)
		hardscroll += 10;
	if (hardscroll > (grid->row - tty.HEIGHT))
		hardscroll = 0;
	grid->changed = true;
}

void Runtime::doscrollup()
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

void Runtime::docommandmode()
{
	if (!in_commandmode())
		doclearline();
	set_commandmode(true);
}

void Runtime::connected()
{
	Runtime *conn = this;

	conn->grid->infof(_("/// connected with {}\n"), ssl ? "telnets" : "telnet");

	static int printed_escape_line = 0;
	if (!printed_escape_line)
	{
		conn->grid->infof(_("/// escape character is '{}'\n"), "^]");
		printed_escape_line = 1;
	}

	tty.title(fmt::format(_("{}://{}:{} - Crystal"), conn->ssl ? "telnets" : "telnet", conn->host, conn->port));

	queue_repaint();
}

bool Runtime::file_log(const std::string &filename)
{
	if (logfile)
	{
		grid->info(_("/// closing old logfile.\n"));
		fclose(logfile);
		logfile = NULL;
	}
	logfile = fopen(filename.c_str(), "a");
	if (!logfile)
	{
		grid->infof(_("/// couldn't open '{}' for appending.\n"), filename);
		return false;
	}
	else
	{
		grid->infof(_("/// logging to end of '{}'.\n"), filename);
		return true;
	}
}

bool Runtime::disconnected(int bts, int pend)
{
	Runtime *conn = this;
	tty.title(_("Disconnected - Crystal"));
	if (conn->grid->col)
		conn->grid->newline();

	conn->grid->info(_("/// connection closed by foreign host.\n"));

	fflush(stdout);
	asio::error_code ignored;
	socket_->close(ignored);
	conn->telnet.reset();

	if (!reconnecting)
		conn->set_commandmode(true);

	queue_repaint();

	return false;
}

void Runtime::queue_repaint()
{
	asio::post(io_,
		   [self = shared_from_this()]
		   {
			   self->grid->changed = true;
			   self->display_buffer();
		   });
}

void do_read(Runtime *conn, asio::posix::stream_descriptor &stream_desc, std::array<char, 256> &buffer);

void handle_input(Runtime *conn, const asio::error_code &error, size_t bytes_transferred, asio::posix::stream_descriptor &stream_desc,
		  std::array<char, 256> &buffer)
{
	if (!error)
	{
		for (size_t idx = 0; idx < bytes_transferred; idx++)
		{
			String32 s = tty.convert_input(buffer[idx]);
			if (s.length())
				conn->dispatch_key(s);
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

void do_read(Runtime *conn, asio::posix::stream_descriptor &stream_desc, std::array<char, 256> &buffer)
{
	stream_desc.async_read_some(asio::buffer(buffer),
				    [conn, &stream_desc, &buffer](const asio::error_code &error, size_t bytes_transferred)
				    { handle_input(conn, error, bytes_transferred, stream_desc, buffer); });
}

void Runtime::main_loop(asio::io_context &io_context)
{
	grid->changed = true;
	tty.bad_have = true;

	asio::posix::stream_descriptor stdin_desc(io_context, STDIN_FILENO);

	std::array<char, 256> input_buffer;

	assert(!cursor);

	display_buffer();
	fflush(stdout);

	do_read(this, stdin_desc, input_buffer);

	io_context.run();

	stdin_desc.release();
}

void Runtime::connect(const std::string &host, const std::string &port, bool ssl)
{
	this->host = host;
	this->port = atoi(port.c_str());

	this->ssl = ssl;

	reconnecting = true;

	socket_ = std::make_unique<tcp::socket>(io_);

	if (ssl)
		ssl_stream_ = std::make_unique<asio::ssl::stream<tcp::socket &>>(*socket_, ssl_ctx_);
	else
		ssl_stream_.reset();

	grid->infof("/// resolving {}\n", host);
	grid->changed = true;

	resolver_.async_resolve(host, port,
				[self = shared_from_this()](asio::error_code ec, auto results)
				{
					if (ec)
					{
						self->reconnecting = false;
						return self->fail("resolve", ec);
					}

					for (auto const &entry : results)
					{
						auto endpoint = entry.endpoint();
						std::string ip = endpoint.address().to_string();
						unsigned short port = endpoint.port();
						self->grid->infof("/// connecting to {}:{}\n", ip, port);
					}

					self->grid->changed = true;
					self->display_buffer();

					asio::async_connect(*self->socket_, results,
							    [self](auto ec, auto)
							    {
								    if (ec)
								    {
									    self->reconnecting = false;
									    return self->fail("connect", ec);
								    }

								    if (self->ssl)
								    {
									    self->ssl_stream_->async_handshake(
										asio::ssl::stream_base::client,
										[self](auto ec)
										{
											if (ec)
											{
												self->reconnecting = false;
												return self->fail("handshake", ec);
											}
											self->on_connected();
											self->reconnecting = false;
										});
								    }
								    else
								    {
									    self->on_connected();
									    self->reconnecting = false;
								    }
							    });
				});

	set_commandmode(false);
	display_buffer();
}

void Runtime::on_connected()
{
	connected();

	telnet = std::make_shared<TelnetState>(*socket_.get(), ssl_stream_.get());

	do_read_socket();
}

void Runtime::do_read_socket()
{
	auto self = shared_from_this();

	auto handler = [self](auto ec, std::size_t n)
	{
		if (ec)
			return self->fail("read", ec);

		std::string data(self->socket_raw_.data(), n);

		const char *data2 = data.data();

		self->telnet->handle_read(self.get(), (unsigned char *)data2, data.size());

		if (self->grid->changed)
			self->display_buffer();

		self->do_read_socket();
	};

	if (ssl)
		ssl_stream_->async_read_some(asio::buffer(socket_raw_), handler);
	else
		socket_->async_read_some(asio::buffer(socket_raw_), handler);
}

void Runtime::fail(const std::string &what, asio::error_code ec)
{
	grid->infof("/// connection failed: {}\n", ec.message());
	disconnected(0, 0);
}

void Runtime::set_commandmode(bool new_command_mode)
{
	if (in_commandmode() == new_command_mode)
		return;

	CommandEditor::set_commandmode(new_command_mode);
}

struct termios oldti;
