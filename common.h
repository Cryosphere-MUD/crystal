#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <stdio.h>
#include <curses.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <libintl.h>

extern struct termios oldti;

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

#define COL_BLACK   0
#define COL_RED     1
#define COL_GREEN   2
#define COL_YELLOW  3
#define COL_BLUE    4
#define COL_MAGENTA 5
#define COL_CYAN    6
#define COL_WHITE   7
#define COL_DEFAULT 9

#define BEL '\a'
#define SI  0x0e
#define SO  0x0f

#define LF  '\n'
#define VT  '\v'
#define FF  '\f'
#define CR  '\r'
#define BS  '\b'

#define ESC L'\033'
#define CSI L'\233'

#define ST 0x9c

size_t real_wcwidth(wchar_t ucs);

enum Int {
  I_UNK = -1,
  I_DIM = 0,
  I_NORM,
  I_BOLD,
};

struct cell_t { 
  wchar_t ch;
  Int inten;
  unsigned char fc;
  bool scs:1;
  unsigned char bc;
  bool ul:1;
  bool it:1;
  bool fr:1;
  bool os:1;
  bool inv:1;
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
    return true;
  }
  bool operator != (const cell_t &o) const {
    return !operator==(o);
  }
};


inline cell_t mkchar(wchar_t ch, Int inten, int fc, int bc, int scs, int ul, int it, int fr, int os, int in)
{
  cell_t i = { ch, inten, fc, scs, bc, ul, it, fr, os, in };
  return i;
}

inline cell_t mkchar(wchar_t ch)
{
  cell_t i = { ch, I_NORM, COL_DEFAULT, 0, COL_DEFAULT, 0, 0, 0, 0, 0 };
  return i;
}

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
