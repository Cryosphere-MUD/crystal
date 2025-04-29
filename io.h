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

#ifndef IO_H
#define IO_H

#include <string.h>

#include "common.h"

struct mterm {
  bool utf8;
  bool xterm_title;
  bool col256;
  bool knowscroll;
  std::string curtitle;
  bool titleset;

  int ccs;
  int cfg;
  int cbg;
  Intensity cint;
  int cit;
  int cfr;
  int cinv;
  int cos;
  int cul;
  bool died;

  void initcol()
  {
    cfg = -1;
    cbg = -1;
    cint = I_UNK;
    cul = -1;
    cit = -1;
    cfr = -1;
    cinv = -1;
    cos = -1;
    died = false;
  }

  mterm() 
  : utf8(false)
  , xterm_title(false)
  , col256(false)
  , knowscroll(false)
  , curtitle("")
  , titleset(false)
  , acsc("")
  , acsc_set(0)
  , bad_have(true)
  {
    HEIGHT = 24;
    WIDTH = 80;
    initcol();
    memset(evillines, 0, sizeof(evillines));
  }

  void beep() {
    printf("\a");
    fflush(stdout);
  }

  void title(const char *fmt, ...) {
    if (xterm_title) {
      char buf[1000];
      va_list a;
      va_start(a, fmt);
      vsprintf(buf, fmt, a);
      if (!titleset || curtitle != buf) {
	printf("\033]2;%s\a", buf);
	curtitle = buf;
	titleset = 1;
      }
      va_end(a);
    }
  }
  
  std::string getinfo(const char *name, const char *def=0) {
    const char *i = tigetstr((char*)name);
    if (i && strstr(i, "$<")) {
      std::string q = i;
      q = q.substr(0, strstr(i, "$<")-i);
      return q;
    }
    if (i) 
      return i;
    return def;
  }

  std::string acsc;
  int acsc_set;

  void outvtchar(unsigned char w) {
    if (!acsc_set) {
      acsc = getinfo("acsc", "");
      acsc_set = 1;
    }
    const char *s = acsc.c_str();
    while (*s) {
      if (*s==w) {
	printf("%c", s[1]);
	return;
      }
      s += 2;
    }

    const char *def = "+#????\\+?#+++++~--_++++++<>*!Lo";
    if (w >= '`' && w <= 0x7e) {
      printf("%c", def[w-'`']);
      return;
    }

    printf("%c", '?');
  }

  int HEIGHT;
  int WIDTH;

  void grabwinsize() {
    struct winsize wws;
    const struct winsize *ws = &wws;
    ioctl(0, TIOCGWINSZ, ws);
    if (ws->ws_row) 
      HEIGHT = MIN(ws->ws_row,     MAXHEIGHT)-1;
    if (ws->ws_col) 
      WIDTH = MIN(ws->ws_col, MAXWIDTH);    
  }

  void getterm() {
    const char *t=getenv("TERM");
    if (t && (strcmp(t, "xterm")==0||
	      strcmp(t, "rxvt")==0||
	      strcmp(t, "xterm-color")==0||
	      strcmp(t, "screen")==0)||
	strncmp(t, "xterm-", 5)==0) {
      xterm_title = 1;
      knowscroll = 0;
      if (strstr(t, "256") || getenv("TERM256"))
	col256 = 1;
    }
  }

  void plonk(const cell_t &g, bool allow_dead);

  cell_t wantbuffer[MAXHEIGHT][MAXWIDTH];
  bool bad_have;
  int evillines[MAXHEIGHT];

  void show_want();

  my_wstring convert_input(int i);
  int getinput();

private:
  cell_t havebuffer[MAXHEIGHT][MAXWIDTH];


};

#endif
