#ifndef GRID_H
#define GRID_H

#include "common.h"
#include "io.h"
#include "crystal.h"
#include "scripting.h"

#include <list>
#include <vector>

struct ansi_context 
{
  int mode;
  std::list<int> pars;
  int par;
  Int inten;
  int forecol, backcol, scs, ul, it, fr, inv, os, hidden, lastcr;
  std::string title;
  int defbc, deffc;

  void init() {
    mode = 0;
    pars = std::list<int>();
    par = 0;
    inten = I_NORM;
    deffc = forecol = COL_DEFAULT;
    defbc = backcol = COL_DEFAULT;
    scs = 0;
    ul = 0;
    it = 0;
    fr = 0;
    inv = 0;
    os = 0;
    hidden = 0;
    lastcr = 0;
    title = "";
  }
};

struct line_t {
  cell_t i[MAXWIDTH];
  int len;
};

#define GRIDHEIGHT 10000

class grid_t : public ansi_context {
public:
  conn_t *conn;

  std::vector<line_t> lines;
  int first;
  int n;
  int lcount;

  int row;
  int col;

  int lastprompt;

  int height;
  int width;

  int visible;

  int changed;

  cellstring cstoredprompt;  
  
  void set_conn(conn_t *c) {
    conn = c;
  }

  grid_t(conn_t *c) {
    conn = 0;
    first = 0;
    n = 0;
    lcount = GRIDHEIGHT;
    lines.reserve( lcount );
    lastprompt = 0;
    nlw = 0;
    row = 0;
    visible = 1;
    changed = 0;

    height = 0;
    width = 0;

    init();
  }

  cell_t myblank() {
    cell_t b = blank;
    b.fc = deffc;
    b.bc = defbc;
    return b;
  }

  cell_t get(int r, int c) {
    if (r < first)
      return myblank();
    if (r > (first + n))
      return myblank();

    if (c > lines[r % lcount].len)
      return myblank();

    return lines[r % lcount].i[c];
  }

  void newline() {
    my_wstring s;
    for (int i=0;i<get_len(row);i++) {
      s += get(row, i).ch;
    }

    if (conn->logfile) {
      for (int i=0;i<get_len(row);i++) {
	fprintf(conn->logfile, "%lc", get(row,i).ch);
      }
      fprintf(conn->logfile, "\n");
      fflush(conn->logfile);
    }

    row++;
    col = 0;
    n++;
    for (int i=0;i<MAXWIDTH;i++) {
      lines [ row % lcount ].i[i] = myblank();
    }
    if (n > lcount) {
      first++;
      n--;
    }
    scripting::dotrigger(s);
  }

  void eraseline(int from) 
  {
    for (int i=from;i<MAXWIDTH;i++) {
      lines [ row % lcount ].i[i] = blank;
    }
  }

  int get_len(int r) {
    if (r < first)
      return 0;
    if (r > (first + n))
      return 0;

    return lines[r % lcount].len;
  }

  void set(int r, int c, cell_t ch) {
    if (lines [ r % lcount].len <= c) 
      lines[r % lcount].len = c+1;
    lines[r % lcount].i[c] = ch;
  }

  int nlw;

  void wantnewline() {
    nlw++;
  }

  void place(const cell_t *ri);

  void wterminal(wchar_t ch);
  void show_batch(const cellstring &batch);
};

inline void have_prompt(grid_t *grid) {
  grid->lastprompt = 1;

  my_wstring s;
  for (int i=0;i<grid->get_len(grid->row);i++) {
    s += grid->get(grid->row, i).ch;
  }
  scripting::doprompt(s);
}

extern int info_to_stderr;

void info(grid_t *g, const char *);
void info(grid_t *g, const wchar_t *);
void info(grid_t *g, const my_wstring &);

void infof(grid_t *g, const char *fmt, ...) /* __attribute__ (( format (printf, 2, 3) )) */;

#endif
