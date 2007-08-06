#ifndef IO_H
#define IO_H

#include "common.h"

struct mterm {
  int utf8;
  int xterm_title;
  int col256;
  int knowscroll;
  std::string curtitle;
  int titleset;

  int ccs;
  int cfg;
  int cbg;
  Int cint;
  int cit;
  int cfr;
  int cinv;
  int cos;
  int cul;
  int died;

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
    died = 0;
  }

  mterm() : utf8(0), xterm_title(0), col256(0), knowscroll(0), curtitle(""),
  titleset(0), acsc(""), acsc_set(0) {
    HEIGHT = 24;
    WIDTH = 80;
    initcol();
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
	printf("\033]0;%s\a", buf);
	curtitle = buf;
	titleset = 1;
      }
      va_end(a);
    }
  }
  
  std::string getinfo(char *name, const char *def=0) {
    const char *i = tigetstr(name);
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

  my_wstring convert_input(int i);
  int getinput();
};

#endif
