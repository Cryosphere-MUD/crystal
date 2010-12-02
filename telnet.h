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

#include "common.h"
#include "crystal.h"
#include "grid.h"
#include "config.h"

#ifdef HAVE_ZLIB
#define MCCP
#endif

#ifdef MCCP
#include "mccpDecompress.h"
#endif

struct telnet_state {
  Socket *s;

  int will_eor;
  int allstars;
  int will_echo;
  int charmode;
  int rcvd_iac;
  int mode;
  int subneg_type;
  int will_ttype;
  std::string subneg_data;
  int do_naws;
#ifdef MCCP
  mc_state *mc;
#endif
  telnet_state(Socket *sock) : 
    s(sock),
    will_eor(0),
    allstars(0),
    will_echo(0),
    charmode(0),
    rcvd_iac(0),
    mode(0),
    subneg_type(0),
    will_ttype(0),
    subneg_data(""),
    do_naws(0)
#ifdef MCCP
    , mc(0)
#endif
{
#ifdef MCCP
    mc = mudcompress_new();
#endif
  }
  
  ~telnet_state() {
#ifdef MCCP
    if (mc) {
      mudcompress_delete(mc);
      mc = 0;
    }
#endif
    if (s) {
      delete s;
      s = 0;
    }
  }

  void tstack(conn_t *conn, int ch);
  void decompress(conn_t *, unsigned char *, size_t);

  void send(const std::string &s);

  void reply(int a, int b, int c);

  void subneg_send(int neg, const std::string &data);
};

void sendwinsize(conn_t *);

#endif
