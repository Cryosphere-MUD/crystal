/*
 * Crystal Mud Client
 * Copyright (C) 2002-2025 Abigail Brady
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

#define TELCMDS
#define TELOPTS
#include <arpa/telnet.h>
#undef TELCMDS
#undef TELOPTS

#include <nlohmann/json.hpp>

#include "Socket.h"
#include "io.h"
#include "telnet.h"

#include <errno.h>
#include <iconv.h>
#include <sstream>

// #undef debug_fprintf
// #define debug_fprintf(a) fprintf a

#define TELOPT_MXP 0x5b
#define TELOPT_MPLEX 0x70
#define MPLEX_SELECT 0x71
#define MPLEX_HIDE 0x72
#define MPLEX_SHOW 0x73
#define MPLEX_SETSIZE 0x74

#define TELOPT_GMCP 201

#define TELOPT_COMPRESS2 86
#define TELOPT_COMPRESS4 88
#define MCCP4_ACCEPT_ENCODING 1
#define MCCP4_BEGIN_ENCODING 2

#undef NEGOTIATE_MXP

extern mterm tty;

std::string nam(int i);

void sendwinsize(conn_t *conn)
{
	if (!conn->telnet)
		return;

	if (!conn->telnet->do_naws)
		return;

	std::string str;

	int wide = tty.WIDTH;
	int high = tty.HEIGHT + 1;

	str += (wide >> 8);
	str += (wide & 0xff);
	str += (high) >> 8;
	str += (high)&0xff;

	conn->telnet->subneg_send(TELOPT_NAWS, str);
}

std::string nam(int s)
{
	if (s > xEOF)
		return telcmds[s - xEOF];
	if (s < NTELOPTS)
		return telopts[s];
	char q[4];
	sprintf(q, "%02x", s);
	return q;
}

void telnet_state::reply(int a, int b, int c)
{
	char buf[5];
	buf[0] = a;
	buf[1] = b;
	buf[2] = c;
	asio::write(s, asio::buffer(buf, 3));
	debug_fprintf((stderr, "SEND %s %s %s\n", nam(a).c_str(), nam(b).c_str(), nam(c).c_str()));
}

int flag_table[] = {
    0x2423, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, 0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x2736,
    0x25b8, 0x25c2, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8, 0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc,
};

void decode(conn_t *conn, grid_t *grid, int ch)
{
	int pass = (ch >= 0x20 && ch <= 0x7f) || ch == 033 || ch == '\n' || ch == '\r' || ch == 0;

	if (pass || conn->mud_cset == "ISO-8859-1")
	{
		grid->wterminal(ch);
		return;
	}

	static iconv_t i = 0;
	static std::string cur_cset = "";

	if (conn->mud_cset == "CP437" && ch <= 0x20)
	{
		grid->wterminal(flag_table[ch]);
		return;
	}

	static char ibuf[10] = {0};
	iconv_inptr_t in = ibuf;
	static size_t inb = 0;

	if (!i || cur_cset != conn->mud_cset)
	{
		if (i)
			iconv_close(i);
		i = iconv_open("WCHAR_T", conn->mud_cset.c_str());
		cur_cset = conn->mud_cset;
		ibuf[0] = 0;
		inb = 0;
	}

	if (!i)
		grid->wterminal('?');

	ibuf[inb++] = ch;
	if (inb > 9)
		inb = 9;

	char obuf[5];
	char *out = obuf;
	size_t outb = 4;

	if (iconv(i, &in, &inb, &out, &outb) == (size_t)-1)
	{
		if (errno == EILSEQ)
		{
			for (size_t id = 0; id < inb; id++)
				grid->wterminal(0xfffd);
			inb = 0;
			iconv(i, NULL, NULL, NULL, NULL);
		}
	}

	if (outb == 0)
		grid->wterminal(*((wchar_t *)obuf));
	return;
}

void telnet_state::handle_read(conn_t *conn, unsigned char *bytes, size_t len)
{
	for (int i = 0; i < len; i++)
	{
#ifdef MCCP4
		if (compression_mode == TELOPT_COMPRESS4)
			mccp4_state.input_buffer += bytes[i];
#endif

#ifdef MCCP

		if (compression_mode == TELOPT_COMPRESS2)
			zlib_state.input_buffer += bytes[i];
#endif

		if (!compression_mode)
			conn->telnet->tstack(conn, bytes[i]);
	}

#ifdef MCCP4
	if (compression_mode == TELOPT_COMPRESS4)
	{
		std::string &input_buffer = mccp4_state.input_buffer;
		ZSTD_inBuffer_s inbuffer = {input_buffer.data(), input_buffer.size(), 0};

		std::vector<unsigned char> outdata(ZSTD_DStreamOutSize());

		ZSTD_outBuffer_s outbuffer;

		int rval;

		do
		{

			outbuffer = {outdata.data(), outdata.size(), 0};

			rval = ZSTD_decompressStream(mccp4_state.stream, &outbuffer, &inbuffer);

			if (ZSTD_isError(rval))
			{
				conn->grid->info("/// Compression error!\n");
				ZSTD_freeDStream(mccp4_state.stream);
				mccp4_state.stream = nullptr;
				compression_mode = 0;
				return;
			}

			input_buffer = input_buffer.substr(inbuffer.pos);
			for (int idx = 0; idx < outbuffer.pos; idx++)
				conn->telnet->tstack(conn, outdata[idx]);
		}
		while (outbuffer.pos == outbuffer.size);

		if (rval == 0)
		{
			compression_mode = 0;
			std::string remainder = input_buffer;
			handle_read(conn, (unsigned char *)remainder.data(), remainder.size());
			ZSTD_freeDStream(mccp4_state.stream);
			mccp4_state.stream = nullptr;
		}
	}
#endif

#ifdef MCCP
	if (compression_mode == TELOPT_COMPRESS2)
	{
		std::string &input_buffer = zlib_state.input_buffer;
		z_stream &stream = zlib_state.stream;

		int status = 0;
		size_t obtained = 0;

		do
		{
			stream.next_in = (Bytef *)input_buffer.data();
			stream.avail_in = input_buffer.size();

			std::vector<Bytef> outdata(16384);

			auto first_out = outdata.data();
			stream.next_out = outdata.data();
			stream.avail_out = outdata.size();

			int status = inflate(&stream, Z_PARTIAL_FLUSH);

			obtained = stream.next_out - first_out;

			input_buffer = std::string((const char *)stream.next_in, stream.avail_in);
			while (first_out < stream.next_out)
			{
				conn->telnet->tstack(conn, *first_out);
				first_out++;
			}

			if (status == Z_STREAM_END)
			{
				compression_mode = 0;
				handle_read(conn, stream.next_in, stream.avail_in);
			}
		}
		while (status == Z_OK && obtained);
	}
#endif
}

#ifdef MCCP
void telnet_state::handle_compress2(conn_t *conn)
{
	compression_mode = TELOPT_COMPRESS2;
	zlib_state = {};
	inflateInit(&zlib_state.stream);
}
#endif

#ifdef MCCP4
void telnet_state::handle_compress4(conn_t *conn)
{
	if (subneg_data[0] == MCCP4_BEGIN_ENCODING)
	{
		if (subneg_data.substr(1) == "zstd")
		{
			compression_mode = TELOPT_COMPRESS4;
			if (mccp4_state.stream)
				ZSTD_freeDStream(mccp4_state.stream);
			mccp4_state.input_buffer = "";
			mccp4_state.stream = ZSTD_createDStream();
			ZSTD_initDStream(mccp4_state.stream);
		}
		else
		{
			conn->grid->info("unknown compression method\n");
		}
	}
}
#endif

void telnet_state::handle_ttype(conn_t *conn)
{
	std::string str;
	str += (char)TELQUAL_IS;
	if (ttype_count == 0)
	{
		str += (conn->mud_cset == "UTF-8" ? "ucryotel" : "cryotel");
	}
	else if (ttype_count == 1)
	{
		str += "crystal:000_003_001";
	}
	else if (ttype_count == 2 || ttype_count == 3)
	{
		int mtts_bitmask = 0;
		mtts_bitmask |= 1; // ANSI
		if (conn->mud_cset == "UTF-8")
			mtts_bitmask |= 4; // UTF-8

		if (tty.col256)
		{
			mtts_bitmask |= 8;   // 256 colors
			mtts_bitmask |= 256; // TrueColor
		}

#ifdef HAVE_SSL
		mtts_bitmask |= 2048; // SSL
#endif

		std::stringstream ss;
		ss << "MTTS ";
		ss << mtts_bitmask;

		str += ss.str();
	}
	ttype_count++;
	subneg_send(TELOPT_TTYPE, str);
}

void telnet_state::handle_mplex(conn_t *conn)
{
	if (subneg_data.length() == 2)
	{
		if (subneg_data[0] == MPLEX_SELECT)
			conn->cur_grid = subneg_data[1] ? conn->overlay : conn->grid;
		if (subneg_data[0] == MPLEX_HIDE)
		{
			if (subneg_data[1] == 1 && conn->overlay)
			{
				conn->overlay->visible = 0;
				conn->overlay->changed = 1;
				conn->grid->changed = 1;
			}
		}
		if (subneg_data[0] == MPLEX_SHOW)
		{
			if (subneg_data[1] == 1 && conn->overlay)
			{
				conn->overlay->visible = 1;
				conn->overlay->changed = 1;
				conn->grid->changed = 1;
			}
		}
	}
	if (subneg_data.length() == 6 && subneg_data[0] == MPLEX_SETSIZE)
	{
		if (subneg_data[1] == 1 && conn->overlay)
		{
			conn->overlay->width = (subneg_data[2] << 8) | (subneg_data[3]);
			conn->overlay->height = (subneg_data[4] << 8) | (subneg_data[5]);
			conn->overlay->changed = 1;
		}
	}
}

void telnet_state::tstack(conn_t *conn, int ch)
{
	if (mode == IAC)
	{
		debug_fprintf((stderr, "%s ", nam(ch).c_str()));

		if (ch == EOR || ch == GA)
		{
			will_eor = 1;
			have_prompt(conn->grid);
		}
		if (ch == NOP)
		{
			mode = 0;
			debug_fprintf((stderr, "\n"));
			return;
		}
		if (ch == IAC)
		{
			if (subneg_type)
				subneg_data += ch;
			else
				decode(conn, conn->cur_grid, 0xff);
			mode = 0;
			debug_fprintf((stderr, "\n"));
			return;
		}
		if (ch == WILL || ch == WONT || ch == DO || ch == DONT || ch == SB)
		{
			mode = ch;
			return;
		}
		if (ch == SE)
		{
			if (subneg_type == TELOPT_TTYPE)
				handle_ttype(conn);
			if (subneg_type == TELOPT_MPLEX)
				handle_mplex(conn);
#ifdef MCCP
			if (subneg_type == TELOPT_COMPRESS2)
				handle_compress2(conn);
#endif
#ifdef MCCP4
			if (subneg_type == TELOPT_COMPRESS4)
				handle_compress4(conn);
#endif
			subneg_type = 0;
			mode = 0;
			debug_fprintf((stderr, "\n"));
			return;
		}
		mode = 0;
		debug_fprintf((stderr, "\n"));
		return;
	}

	if (mode == SB)
	{
		debug_fprintf((stderr, "%s ", nam(ch).c_str()));
		subneg_data = "";
		subneg_type = ch;
		subneg_data = "";
		mode = 0;
		debug_fprintf((stderr, "\n"));
		return;
	}

	if (mode == WILL)
	{
		debug_fprintf((stderr, "%s ", nam(ch).c_str()));
		if (ch == TELOPT_MPLEX)
		{
			reply(IAC, DO, TELOPT_MPLEX);
			mode = 0;
			debug_fprintf((stderr, "\n"));
			return;
		}

		if (ch == TELOPT_ECHO)
		{
			allstars = 1;
			will_echo = 1;
			mode = 0;
			reply(IAC, DO, TELOPT_ECHO);
			debug_fprintf((stderr, "\n"));
			return;
		}

		if (ch == TELOPT_GMCP)
		{
			gmcp = true;
			mode = 0;
			reply(IAC, DO, TELOPT_GMCP);
			nlohmann::json hello;
			hello["Client"] = "Crystal";
			hello["Version"] = VERSION;
			subneg_send(TELOPT_GMCP, hello.dump());
			debug_fprintf((stderr, "\n"));
			return;
		}

#ifdef WILL_LINEMODE
		if (!rcvd_iac)
			reply(IAC, WILL, TELOPT_LINEMODE);
#endif
		rcvd_iac = 1;

		if (ch == TELOPT_COMPRESS4)
		{
			reply(IAC, DO, TELOPT_COMPRESS4);
			std::string encodings;
			encodings.push_back(MCCP4_ACCEPT_ENCODING);
			encodings += "zstd";
			subneg_send(TELOPT_COMPRESS4, encodings);
			mode = 0;
			return;
		}

		if (ch == TELOPT_COMPRESS2)
		{
			reply(IAC, DO, TELOPT_COMPRESS2);
			mode = 0;
			return;
		}

		if (ch == TELOPT_EOR)
		{
			debug_fprintf((stderr, "\n"));
			if (!will_eor)
			{
				reply(IAC, DO, TELOPT_EOR);
				will_eor = 1;
			}
			mode = 0;
			return;
		}

		if (ch == TELOPT_SGA)
		{
			reply(IAC, DO, TELOPT_SGA);
			mode = 0;
			charmode = 1;
			return;
		}
#ifdef NEGOTIATE_MXP
		if (ch == TELOPT_MXP)
		{
			reply(IAC, DO, TELOPT_MXP);
			mode = 0;
			return;
		}
#endif
		if (ch == TELOPT_BINARY)
		{
			reply(IAC, DO, TELOPT_BINARY);
			mode = 0;
			return;
		}

		reply(IAC, DONT, ch);

		mode = 0;
		debug_fprintf((stderr, "\n"));
		return;
	}

	if (mode == WONT)
	{
		debug_fprintf((stderr, "%s ", nam(ch).c_str()));
		if (ch == TELOPT_ECHO)
		{
			allstars = 0;
			will_echo = 0;
			reply(IAC, DONT, TELOPT_ECHO);
		}
		else
		{
#ifdef WILL_LINEMODE
			if (!rcvd_iac)
				reply(IAC, WILL, TELOPT_LINEMODE);
#endif
			rcvd_iac = 1;
		}

		mode = 0;
		debug_fprintf((stderr, "\n"));
		return;
	}

	if (mode == DO)
	{
		debug_fprintf((stderr, "%s ", nam(ch).c_str()));

		if (ch == TELOPT_LINEMODE)
		{
			charmode = 0;
			mode = 0;
		}

#ifdef WILL_LINEMODE
		if (!rcvd_iac)
			reply(IAC, WILL, TELOPT_LINEMODE);
#endif
		rcvd_iac = 1;

		if (ch == TELOPT_SGA)
		{
			reply(IAC, WILL, TELOPT_SGA);
			mode = 0;
			return;
		}

		if (ch == TELOPT_NAWS)
		{
			debug_fprintf((stderr, "\n"));

			if (!do_naws)
			{
				reply(IAC, WILL, TELOPT_NAWS);
				do_naws = 1;
				sendwinsize(conn);
			}

			mode = 0;
			return;
		}
		if (ch == TELOPT_TTYPE)
		{
			debug_fprintf((stderr, "\n"));
			if (!will_ttype)
			{
				reply(IAC, WILL, TELOPT_TTYPE);
				will_ttype = 1;
			}
			mode = 0;
			return;
		}

		if (ch == TELOPT_BINARY)
		{
			reply(IAC, WILL, TELOPT_BINARY);
			mode = 0;
			return;
		}

		debug_fprintf((stderr, "\n"));

		reply(IAC, WONT, ch);

		mode = 0;
		return;
	}

	if (mode == DONT)
	{
		debug_fprintf((stderr, "%s ", nam(ch).c_str()));
		debug_fprintf((stderr, "\n"));

#ifdef WILL_LINEMODE
		if (!rcvd_iac)
			reply(IAC, WILL, TELOPT_LINEMODE);
#endif
		rcvd_iac = 1;
#ifdef WILL_LINEMODE
		if (ch == TELOPT_LINEMODE)
			charmode = 1;
#endif
		mode = 0;
		return;
	}

	if (ch == IAC)
	{
		debug_fprintf((stderr, "RECV %s ", nam(ch).c_str()));
		mode = IAC;
		return;
	}

	if (!subneg_type)
	{
		decode(conn, conn->cur_grid, ch);
	}
	else
	{
		debug_fprintf((stderr, "%c", ch));
		subneg_data += ch;
	}
}

void telnet_state::send(const std::string &proper)
{
	std::string p2;
	const unsigned char *b = (const unsigned char *)proper.c_str();
	while (*b)
	{
		if (*b == IAC)
			p2 += IAC;
		p2 += *b;
		b++;
	}
	asio::write(s, asio::buffer(p2.data(), p2.size()));
}

void telnet_state::subneg_send(int subneg, const std::string &proper)
{
	std::string s2;
	s2 += (unsigned char)IAC;
	s2 += (unsigned char)SB;
	s2 += (unsigned char)subneg;

	const unsigned char *b = (const unsigned char *)proper.c_str();
	int l = proper.length();
	while (l)
	{
		if (*b == IAC)
			s2 += IAC;
		s2 += *b;
		b++;
		l--;
	}

	s2 += (unsigned char)IAC;
	s2 += (unsigned char)SE;

	asio::write(s, asio::buffer(s2.data(), s2.size()));
}
