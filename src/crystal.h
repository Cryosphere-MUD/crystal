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

#ifndef CRYSTAL_H
#define CRYSTAL_H

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <memory>
#include <set>
#include <string>

#include "commandeditor.h"
#include "common.h"

using asio::ip::tcp;

struct telnet_state;
class grid_t;
class InAddrList;
typedef std::shared_ptr<InAddrList> InAddrListPtr;

class hlist;

class conn_t : public commandeditor_t, public std::enable_shared_from_this<conn_t>
{
      private:
	//! the amount we have scrolled to in the buffer
	int hardscroll = 0;

      public:
	//! are we in never echo mode (default: no)
	bool never_echo = false;

	//! are we in kludge lp prompts mode (default: yes)
	bool lp_prompts = true;

      public:
	//! are we quitting?
	bool quit = false;

      private:
	InAddrListPtr addrs = nullptr;
	int addr_i = 0;

      public:
	grid_t *grid = nullptr;

	grid_t *overlay = nullptr;
	grid_t *cur_grid = nullptr;

	std::shared_ptr<telnet_state> telnet =  nullptr;
	FILE *logfile = nullptr;

	std::string host;
	int port = 0;
	bool ssl = false;

	std::string mud_cset = "ISO-8859-1";

	void dorefresh();

	void doscrollup();
	void doscrolldown();
	void doscrollstart();
	void doscrollend();
	void triggerfn(const char *which);
	void dofindnext();

	void doenter();
	void dosuspend();
	void dotoggleoverlay();
	void docommandmode();

	void show_lines_at(int from, int to, int num);

	conn_t(asio::io_context& io, grid_t *grid);
	~conn_t();

	void initbindings();
	void dispatch_key(const my_wstring &s);
	void addbinding(const wchar_t *key, const char *bind);

	void connect(const std::string &host, const std::string &port, bool ssl);
	bool file_log(const char *filename);
	void do_read_from_socket();

	void display_buffer();

	void queue_repaint();

	bool disconnected(int bts, int pend);
	void connected();
	bool try_addr(const asio::ip::tcp::resolver::results_type& endpoints,
		      std::string host, int port, bool ssl);

	void main_loop(asio::io_context &io_context);

	void fail(const std::string& what, asio::error_code ec);

	void start(const std::string& host, const std::string& port);

	   void on_connected();

	void set_commandmode(bool new_command_mode) override;

    void do_read_socket();

	std::set<my_wstring> hl_matches;

	std::array<char, 4096> stdin_raw_;
	std::array<char, 4096> socket_raw_;

	tcp::resolver resolver_;

	asio::io_context &io_;

	asio::ssl::context ssl_ctx_;

	bool reconnecting = false;

	std::unique_ptr<tcp::socket> socket_;
	std::unique_ptr<asio::ssl::stream<tcp::socket&>> ssl_stream_;
	// asio::ssl::stream<tcp::socket&> ssl_stream_;

	asio::posix::stream_descriptor stdin_;

	asio::streambuf socket_buf_;
	asio::streambuf stdin_buf_;
};

extern int exitValue;

#endif
