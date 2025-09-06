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

#ifndef TELNET_H
#define TELNET_H

#include <arpa/telnet.h>

#include "Socket.h"
#include "common.h"
#include "crystal.h"
#include "grid.h"

#ifdef HAVE_ZLIB
#define MCCP

#include <zlib.h>

struct ZLibState
{
	std::string input_buffer;
	z_stream_s stream;
};
#endif

#ifdef HAVE_ZSTD
#define MCCP4

#include <zstd.h>

struct MCCP4State
{
	std::string input_buffer;
	ZSTD_DStream *stream = nullptr;
};

#endif

struct telnet_state
{
	asio::ip::tcp::socket &s;

	bool gmcp = false;
	bool will_eor = false;
	bool allstars = false;
	bool will_echo = false;
	bool charmode = false;
	bool rcvd_iac = false;
	int mode = 0;
	int subneg_type = 0;
	int will_ttype = 0;
	int ttype_count = 0;
	std::string subneg_data;
	bool do_naws = false;

	int compression_mode = 0;

#ifdef MCCP
	ZLibState zlib_state;
#endif
#ifdef MCCP4
	MCCP4State mccp4_state;
#endif

	telnet_state(asio::ip::tcp::socket &sock) : s(sock), subneg_data("")
	{
	}

	~telnet_state()
	{
	}

	void tstack(conn_t *conn, int ch);

	void handle_ttype(conn_t *conn);

	void handle_mplex(conn_t *conn);

#ifdef MCCP
	void handle_compress2(conn_t *conn);
#endif

#ifdef MCCP4
	void handle_compress4(conn_t *conn);
#endif

	void handle_read(conn_t *, unsigned char *, size_t);

	void send(const std::string &s);

	void reply(int a, int b, int c);

	void subneg_send(int neg, const std::string &data);
};

void sendwinsize(conn_t *);

#endif
