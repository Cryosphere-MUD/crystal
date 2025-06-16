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

#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <stdio.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdlib.h>

extern struct termios oldti;

#ifdef __APPLE__
typedef const char* iconv_inptr_t;
#else
typedef char* iconv_inptr_t;
#endif

#define debug_fprintf(a) ;

#undef _
#define _(a) a

#ifdef MIN
#undef MIN
#endif

inline int MIN(int a, int b) {
  if (a>b) return b;
  return a;
}

typedef std::basic_string<wchar_t> my_wstring;

std::string mks(const my_wstring &w);
my_wstring mkws(const char* cmd);

#define COL_BLACK   0
#define COL_RED     1
#define COL_GREEN   2
#define COL_YELLOW  3
#define COL_BLUE    4
#define COL_MAGENTA 5
#define COL_CYAN    6
#define COL_WHITE   7
#define COL_DEFAULT -1

#define TRUE_MARKER (1 << 25)

constexpr int make_truecol(unsigned char r, unsigned char g, unsigned char b) {
        return ((r&0xff) << 16 | (g & 0xff) << 8 | (b & 0xff)) | TRUE_MARKER;
}

constexpr bool is_truecol(unsigned int value) {
        return (value & TRUE_MARKER) == TRUE_MARKER;
}

#define BEL '\a'
#define SI  0x0e
#define SO  0x0f

#define LF  '\n'
#define VT  '\v'
#define FF  '\f'
#define CR  '\r'
#define BS  '\b'

#define OSC ']'

#define ESC L'\033'
#define CSI L'\233'

#define ST 0x9c

size_t real_wcwidth(wchar_t ucs);

enum Intensity {
  I_UNK = -1,
  I_DIM = 0,
  I_NORM,
  I_BOLD,
};

struct cell_t { 
  wchar_t ch;
  Intensity inten;
  int fc;
  int bc;
  bool scs:1;
  int ul:2;
  bool it:1;
  bool fr:1;
  bool os:1;
  bool inv:1;
  bool ol:1;
  void operator= (const cell_t &o) {
    ch = o.ch;
    inten = o.inten;
    fc = o.fc;
    bc = o.bc;
    scs = o.scs;
    ul = o.ul;
    it = o.it;
    fr = o.fr;
    os = o.os;
    inv = o.inv;
    ol = o.ol;
  }
  bool operator==(const cell_t &o) const {
    if (ch != o.ch) return false;
    if (inten != o.inten) return false;
    if (fc != o.fc) return false;
    if (bc != o.bc) return false;
    if (scs != o.scs) return false;
    if (ul != o.ul) return false;
    if (it != o.it) return false;
    if (fr != o.fr) return false;
    if (inv != o.inv) return false;
    if (ol != o.ol) return false;
    return true;
  }
  bool operator != (const cell_t &o) const {
    return !operator==(o);
  }
  cell_t(wchar_t ch = '\0', Intensity inten = I_NORM, int fc = COL_DEFAULT, int bc = COL_DEFAULT, int scs = 0, int ul = 0, int it = 0, int fr = 0, int os = 0, int inv = 0, int ol = 0)
  : ch(ch),
    inten(inten),
    fc(fc),
    bc(bc),
    scs(scs),
    ul(ul),
    it(it),
    fr(fr),
    os(os),
    inv(inv),
    ol(ol)
  {
  }
};

extern const cell_t blank;
extern const cell_t blank2;

#define MAXHEIGHT 200
#define MAXWIDTH 320

class cellstring {
  size_t els;
  size_t siz;
  cell_t *s;
public:
  cellstring() : els(0), siz(0), s(0) {
  }
  void erase() { els = 0; }
  int length() const { return els; }
  void operator+=(const cell_t &c) {
    if ((els+1)>siz) {
      siz *= 2;
      if (siz == 0) 
	siz = 16;
      s = (cell_t*)realloc(s, sizeof(cell_t)*siz);
    }
    s[els] = c;
    els++;
  }
  const cell_t &operator[](size_t i) const {
    if (i >= els) return blank;
    return s[i];
  }
  ~cellstring() {
    if (s) free(s);
  }
};


#endif
