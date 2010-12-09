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

#include <set>
#include <string>

#include "common.h"
#include "commandeditor.h"

struct telnet_state;
class grid_t;
class InAddrList;
class hlist;

class conn_t : public commandeditor_t {
 private:
  //! the amount we have scrolled to in the buffer
  int hardscroll;

 public:
  //! are we in never echo mode (default: no)
  bool never_echo;

  //! are we in kludge lp prompts mode (default: yes)
  bool lp_prompts;

 public:
  //! are we quitting?
  bool quit;

 private:
  InAddrList *addrs;
  int addr_i;

 public:
  std::string host;
  int port;
  bool ssl;

  grid_t *grid;

  grid_t *slave;
  grid_t *cur_grid;

  telnet_state *telnet;
  FILE *logfile;

  std::string mud_cset;

  void dorefresh();

  void doscrollup();
  void doscrolldown();
  void doscrollstart();
  void doscrollend();
  void triggerfn(const char *which);
  void dofindnext();

  void doenter();
  void dosuspend();
  void dotoggleslave();
  void docommandmode();

  void show_lines_at(int from, int to, int num);
  
  ~conn_t();
  conn_t(grid_t *);

  void initbindings();
  void dispatch_key(const my_wstring &s);
  void addbinding(const wchar_t* key, const char* bind);

  void connect(const char *host, int port, bool ssl);
  bool file_log(const char *filename);

  void display_buffer();

  bool disconnected(int bts, int pend);
  void connected();
  bool try_addr(const char *host, int port, bool ssl);

  void main_loop();

  std::set<my_wstring> hl_matches;
};

#endif
