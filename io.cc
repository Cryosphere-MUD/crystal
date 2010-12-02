/*
 * Crystal Mud Client
 * Copyright (C) 2002-2003 Abigail Brady
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

#define _XOPEN_SOURCE

#include "io.h"

#include <ctype.h>
#include <locale.h>
#include <wchar.h>
#include <langinfo.h>
#include <string.h>
#include <unistd.h>

int mterm::getinput() {
  unsigned char buf[10];
  if (read(0, buf, 1)==1) {

    if (utf8) {
      if (buf[0]<0x80)
	return buf[0];

      if (buf[0]>0xc0 && buf[0]<=0xdf) {
	read(0, buf+1, 1);
	buf[2] = 0;

	int ucs = (buf[0] - 0xc0) << 6;
	ucs |= (buf[1] & 63);
	return ucs;
      }
      if (buf[0]>=0xe0) {
	read(0, buf+1, 2);
	buf[3] = 0;

	int ucs = (buf[0] - 0xe0) << 12;
	ucs |= (buf[1] & 63) << 6;
	ucs |= (buf[2] & 63);

	return ucs;
      }
    }
    return buf[0]; 
  } else
   return -1;
}


void mterm::plonk(const cell_t &g, bool allow_dead) {
  if (g.scs != ccs) {
    if (g.scs)
      printf("%s", getinfo("smacs", "\033(0").c_str()); // setmode alternate character set
    else
      printf("%s", getinfo("rmacs", "\033(B").c_str()); // resetmode alternate character set

    ccs = g.scs;
  }

  std::string whats = "";

  if ((g.fc != cfg && g.fc == COL_DEFAULT) ||
      (g.bc != cbg && g.bc == COL_DEFAULT) ||
      (!g.inv && cinv) ||
      (!g.ul && cul) ||
      (!g.it && cit) ||
      (!g.fr && cfr) ||
      (!g.os && cos)) {
    whats += "0;";
    cfg = COL_DEFAULT;
    cbg = COL_DEFAULT; 
    cint = I_NORM;
    cul = 0;
    cit = 0;
    cfr = 0;
    cinv = 0;
    cos = 0;
  }

  if (g.inten != cint) {
    if (g.inten == I_BOLD)
      whats += "1;";
    else if (g.inten == I_DIM)
      whats += "2;";
    else
      whats += "22;";
    
    cint = g.inten;
  }

  if (g.fc != cfg) {
    static char blah[100];
    if (g.fc>7) {
      if (col256)
	sprintf(blah, "38;5;%i;", g.fc);
      else
	sprintf(blah, "31;");
    } else {
      sprintf(blah, "%i;", 30+g.fc);
    }
    whats += blah;
    
    cfg = g.fc;
  }

  if (g.bc != cbg) {
    static char blah[100];
    if (g.bc>7) {
      if (col256)
	sprintf(blah, "48;5;%i;", g.bc);
      else
	sprintf(blah, "41;");
    } else {
      sprintf(blah, "%i;", 40+g.bc);
    }
    whats += blah;
    
    cbg = g.bc;
  }

  if (g.ul != cul) {
    if (g.ul)
      whats += "4;";
    else
      whats += "24;";
    cul = g.ul;
  }

  if (g.it != cit) {
    if (g.it)
      whats += "3;";
    else
      whats += "23;";
    cit = g.it;
    cfr = 0;
  }

  if (g.inv != cinv) {
    if (g.inv)
      whats += "7;";
    else
      whats += "27;";
    cinv = g.inv;
  }

  if (g.os != cos) {
    if (g.os)
      whats += "9;";
    else
      whats += "29;";
    cos = g.os;
  }

  if (g.fr != cfr) {
    if (g.fr)
      whats += "20;";
    else
      whats += "23;";
    cfr = g.fr;
  }

  if (whats.length()) {
    whats = whats.substr(0, whats.length()-1);
    printf("\033[%sm", whats.c_str());
  }
  // set the right colour

  if ((g.ch == 0 && !allow_dead) || (g.ch >= 32 && g.ch < 127)) {
    if (g.scs) {
      outvtchar(g.ch?g.ch:' ');
    } else
      printf("%c", g.ch?g.ch:' ');
  } else if (g.ch==0) {
    if (!died) {
      printf("%s", getinfo("el", "\033[K").c_str()); // clear to end of line
      died = 1;
    }
  } else if ((g.ch < 256 || utf8) && (wcwidth(g.ch)>0)) {
    wchar_t ch = g.ch;
    if (ch < 32)
      ch += 0x2400;
    printf("%lc", ch);
  } else {
    switch (real_wcwidth(g.ch)) {
    case 0:
      break;
    case 1:
      printf("-");
      break;
    case 2:
      printf("--");
      break;
    }
  }
  return;
}

my_wstring mterm::convert_input(int i) {
  static my_wstring sofar = L"";

  if (sofar.length()==0) {
    if (i == 127 && getinfo("kdch1", "") == "\177") {
      return L"delete";
    }

    if (i == 127 || i == '\b') return L"backspace";
    if (i == '\t') return L"tab";
    if (i == '\r') return L"return";
    if (i == '\n') return L"return";
    if (i == '\33') { sofar += i; return L""; }
    if (i < 32) {
      my_wstring s;
      s += L"c-";
      s += tolower(i+'@');
      return s;
    }
    
    my_wstring s;
    s += i;
    return s;
  }

  if (sofar == L"\33" && i == '\33') {
    sofar = L"";
  }

  sofar += i;

  static struct {
    const wchar_t *a;
    const wchar_t *b;
  } keys[] = {
    { L"\033[A", L"up"},
    { L"\033[B", L"down"},
    { L"\033[C", L"right"},
    { L"\033[D", L"left"},
    { L"\033[E", L"stay"},

    { L"\033[2A", L"s-up"},
    { L"\033[2B", L"s-down"},
    { L"\033[2C", L"s-right"},
    { L"\033[2D", L"s-left"},
    { L"\033[2E", L"s-stay"},

    { L"\033[3A", L"m-up"},
    { L"\033[3B", L"m-down"},
    { L"\033[3C", L"m-right"},
    { L"\033[3D", L"m-left"},
    { L"\033[3E", L"m-stay"},

    { L"\033[4A", L"m-s-up"},
    { L"\033[4B", L"m-s-down"},
    { L"\033[4C", L"m-s-right"},
    { L"\033[4D", L"m-s-left"},
    { L"\033[4E", L"m-s-stay"},

    { L"\033[5A", L"c-up"},
    { L"\033[5B", L"c-down"},
    { L"\033[5C", L"c-right"},
    { L"\033[5D", L"c-left"},
    { L"\033[5E", L"c-stay"},

    { L"\033[6A", L"s-c-up"},
    { L"\033[6B", L"s-c-down"},
    { L"\033[6C", L"s-c-right"},
    { L"\033[6D", L"s-c-left"},
    { L"\033[6E", L"s-c-stay"},

    { L"\033[7A", L"m-c-up"},
    { L"\033[7B", L"m-c-down"},
    { L"\033[7C", L"m-c-right"},
    { L"\033[7D", L"m-c-left"},
    { L"\033[7E", L"m-c-stay"},

    { L"\033[8A", L"m-s-c-up"},
    { L"\033[8B", L"m-s-c-down"},
    { L"\033[8C", L"m-s-c-right"},
    { L"\033[8D", L"m-s-c-left"},
    { L"\033[8E", L"m-s-c-stay"},

    { L"\033[Z", L"s-tab"}, // xterm and freebsd

    { L"\033[1~", L"home"},
    { L"\033[2~", L"insert"},
    { L"\033[3~", L"delete"},
    { L"\033[4~", L"end"},
    { L"\033[5~", L"pageup"},
    { L"\033[6~", L"pagedown"},

    { L"\033[L", L"insert"},

    { L"\033[H", L"home"},
    { L"\033[F", L"end"},

    { L"\033[I", L"pageup"},
    { L"\033[G", L"pagedown"},

    { L"\033[2;5~", L"c-insert"},
    { L"\033[5;5~", L"c-pageup"},
    { L"\033[6;5~", L"c-pagedown"},

    { L"\033[7~", L"home"}, // rxvt
    { L"\033[8~", L"end"}, // rxvt

    { L"\033[Oa", L"c-up"}, // rxvt
    { L"\033[Ob", L"c-down"}, // rxvt
    { L"\033[Oc", L"c-right"}, // rxvt
    { L"\033[Od", L"c-left"}, // rxvt

    /* THIS BLOCK RXVT NUMERIC KEYPAD */
    { L"\033[Ok", L"+"}, // rxvt
    { L"\033[Oj", L"*"}, // rxvt
    { L"\033[On", L"delete"}, // rxvt
    { L"\033[Oo", L"/"}, // rxvt
    { L"\033[Om", L"-"}, // rxvt
    { L"\033[Op", L"insert"}, // rxvt
    { L"\033[Oq", L"end"}, // rxvt 
    { L"\033[Or", L"down"}, // rxvt 
    { L"\033[Os", L"pagedown"}, // rxvt 
    { L"\033[Ot", L"left"}, // rxvt 
    { L"\033[Ou", L"stay"}, // rxvt 
    { L"\033[Ov", L"right"}, // rxvt
    { L"\033[Ow", L"home"}, // rxvt
    { L"\033[Ox", L"up"}, // rxvt
    { L"\033[Oy", L"pageup"}, // rxvt

    { L"\033[OM", L"return"}, // rxvt
    /* */

    { L"\033[[A", L"fn.1"}, // linux
    { L"\033[[B", L"fn.2"}, // linux
    { L"\033[[C", L"fn.3"}, // linux
    { L"\033[[D", L"fn.4"}, // linux
    { L"\033[[E", L"fn.5"}, // linux
    { L"\033[P", L"pause"}, // linux

    { L"\033[[M", L"fn.1"}, // freebsd
    { L"\033[[N", L"fn.2"}, // freebsd
    { L"\033[[O", L"fn.3"}, // freebsd
    { L"\033[[P", L"fn.4"}, // freebsd
    { L"\033[[Q", L"fn.5"}, // freebsd
    { L"\033[[R", L"fn.6"}, // freebsd
    { L"\033[[S", L"fn.7"}, // freebsd
    { L"\033[[T", L"fn.8"}, // freebsd
    { L"\033[[U", L"fn.9"}, // freebsd
    { L"\033[[V", L"fn.10"}, // freebsd
    { L"\033[[W", L"fn.11"}, // freebsd
    { L"\033[[X", L"fn.12"}, // freebsd
    { L"\033[[Y", L"s-fn.1"}, // freebsd
    { L"\033[[Z", L"s-fn.2"}, // freebsd
    { L"\033[[a", L"s-fn.3"}, // freebsd
    { L"\033[[b", L"s-fn.4"}, // freebsd
    { L"\033[[c", L"s-fn.5"}, // freebsd
    { L"\033[[d", L"s-fn.6"}, // freebsd
    { L"\033[[e", L"s-fn.7"}, // freebsd
    { L"\033[[f", L"s-fn.8"}, // freebsd
    { L"\033[[g", L"s-fn.9"}, // freebsd
    { L"\033[[h", L"s-fn.10"}, // freebsd
    { L"\033[[i", L"s-fn.11"}, // freebsd
    { L"\033[[j", L"s-fn.12"}, // freebsd
    { L"\033[[k", L"c-fn.1"}, // freebsd
    { L"\033[[l", L"c-fn.2"}, // freebsd
    { L"\033[[m", L"c-fn.3"}, // freebsd
    { L"\033[[n", L"c-fn.4"}, // freebsd
    { L"\033[[o", L"c-fn.5"}, // freebsd
    { L"\033[[p", L"c-fn.6"}, // freebsd
    { L"\033[[q", L"c-fn.7"}, // freebsd
    { L"\033[[r", L"c-fn.8"}, // freebsd
    { L"\033[[s", L"c-fn.9"}, // freebsd
    { L"\033[[t", L"c-fn.10"}, // freebsd
    { L"\033[[u", L"c-fn.11"}, // freebsd
    { L"\033[[v", L"c-fn.12"}, // freebsd
    { L"\033[[w", L"s-c-fn.1"}, // freebsd
    { L"\033[[x", L"s-c-fn.2"}, // freebsd
    { L"\033[[y", L"s-c-fn.3"}, // freebsd
    { L"\033[[z", L"s-c-fn.4"}, // freebsd
    { L"\033[[@", L"s-c-fn.5"}, // freebsd
    { L"\033[[[", L"s-c-fn.6"}, // freebsd
    { L"\033[[\\",L"s-c-fn.7"}, // freebsd
    { L"\033[[]", L"s-c-fn.8"}, // freebsd
    { L"\033[[^", L"s-c-fn.9"}, // freebsd
    { L"\033[[_", L"s-c-fn.10"}, // freebsd
    { L"\033[[`", L"s-c-fn.11"}, // freebsd
    { L"\033[[{", L"s-c-fn.12"}, // freebsd

    { L"\033[11~", L"fn.1"},
    { L"\033[12~", L"fn.2"},
    { L"\033[13~", L"fn.3"},
    { L"\033[14~", L"fn.4"},
    { L"\033[15~", L"fn.5"},
    { L"\033[17~", L"fn.6"},
    { L"\033[18~", L"fn.7"},
    { L"\033[19~", L"fn.8"},
    { L"\033[20~", L"fn.9"},
    { L"\033[21~", L"fn.10"},
    { L"\033[23~", L"fn.11"},
    { L"\033[24~", L"fn.12"},
    { L"\033[25~", L"fn.13"}, // shift-f1 on linux - shift-f3 on xterm
    { L"\033[26~", L"fn.14"},
    { L"\033[28~", L"fn.15"},
    { L"\033[29~", L"fn.16"},
    { L"\033[31~", L"fn.17"},
    { L"\033[32~", L"fn.18"},
    { L"\033[33~", L"fn.19"},
    { L"\033[34~", L"fn.20"},

    { L"\033[23;2~", L"fn.21"}, // shift-11 on xterm
    { L"\033[24;2~", L"fn.22"}, // shift-12 on xterm

    { L"\033OA", L"up"},
    { L"\033OB", L"down"},
    { L"\033OC", L"right"},
    { L"\033OD", L"left"},
    { L"\033OH", L"home"},
    { L"\033OF", L"end"},

    { L"\033OP", L"fn.1"},
    { L"\033OQ", L"fn.2"},
    { L"\033OR", L"fn.3"},
    { L"\033OS", L"fn.4"},

    { L"\033<", L"m-<"},
    { L"\033>", L"m->"},

    { NULL, NULL }
  };

  int can = 0;

  for (int j=0;keys[j].a;j++) {
    if (sofar == keys[j].a) {
      sofar = L"";
      return keys[j].b;
    }
    my_wstring c = keys[j].a;
    if (c.length()>sofar.length())
      c.erase(sofar.length());
    if (c == sofar) {
      can = 1;
    }
  }
  
  if (!can) {

    if (sofar.length()==2 && sofar[0]=='\033') {
      my_wstring s;
      s += L"m-";
      s += sofar[1];
      return s;
    }

    sofar = L"";
    return L"";
  }
  
  return L"";
}
