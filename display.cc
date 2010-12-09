#include "crystal.h"
#include "grid.h"
#include "io.h"
#include "telnet.h"

extern mterm tty;

void conn_t::show_lines_at(int from, int to, int num)
{
  for (int i=0;i<num;i++) {
    if (i+from == grid->row && !(telnet && telnet->charmode)) {
      for (int j=0;j<tty.WIDTH;j++)
	tty.wantbuffer[i+to-1][j] = blank2;
      continue;
    }
    int mw = MIN(tty.WIDTH, grid->get_len(i+from));
    int j;
    if (tty.utf8) mw++;
    for (j=0;j<mw;j++) {
      cell_t g = grid->get(i+from, j);
      tty.wantbuffer[i+to-1][j] = g;
    }

    my_wstring s;
    for (int j=0;j<grid->get_len(i+from);j++) {
      s += grid->get(i+from, j).ch;
    }

    std::set<my_wstring>::iterator it;
    for (it=hl_matches.begin();it!=hl_matches.end();it++) {
      size_t l = s.find(*it);
      while (l != my_wstring::npos) {
	for (size_t j=0;j<it->length();j++)
	  tty.wantbuffer[i+to-1][j+l].inv = !tty.wantbuffer[i+to-1][j+l].inv;
	l = s.find(*it, l+1);
      }
    }
      
    while (j<tty.WIDTH) {
      tty.wantbuffer[i+to-1][j] = blank2;
      j++;
    }
  }
}

void conn_t::display_buffer() {
  if (info_to_stderr) 
    return;

  conn_t* conn = this;

  grid_t &grid = *conn->grid;

  if (!grid.changed && !conn->slave->changed)
    return;

  const wchar_t *a = conn->buffer.data();
  int alen = conn->buffer.length();

  const int col = conn->cursor;
  const wchar_t *txt = a;

  int scroll = 0;

  int start = grid.row - tty.HEIGHT;
  if (start < 0) {
    start = 0;
  }

  if (hardscroll)
    start = hardscroll-1;

  if (conn->telnet && conn->telnet->charmode && !commandmode) {
    if (grid.row<tty.HEIGHT)
      conn->show_lines_at(start, tty.HEIGHT-grid.row+1, tty.HEIGHT+1);
    else
      conn->show_lines_at(start, 1, tty.HEIGHT+1);
    tty.show_want();
    printf("\033[%i;%if", tty.HEIGHT+1, grid.col+1);
    return;
  }

  if (grid.changed) {
    if (hardscroll) {
      conn->show_lines_at(start, 1, (tty.HEIGHT-4));
      conn->show_lines_at((grid.row-5)>0?grid.row-5:0, tty.HEIGHT-4, 5);
      for (int i=0;i<tty.WIDTH;i++) {
	tty.wantbuffer[tty.HEIGHT-5][i] = cell_t('=', I_BOLD, COL_WHITE, COL_RED, 0, 0, 0, 0, 0, 0);
      }
    } else {
      if (grid.row<tty.HEIGHT)
	conn->show_lines_at(start, tty.HEIGHT-grid.row+1, tty.HEIGHT);
      else
	conn->show_lines_at(start, 1, tty.HEIGHT);
    }
    grid.changed = 0;
  }

  if (conn->slave && conn->slave->visible) {
    for (int i=0;i<conn->slave->height;i++) {
      for (int j=0;j<conn->slave->width;j++) {
	if (tty.WIDTH-conn->slave->width+j>=0)
	  tty.wantbuffer[i][tty.WIDTH-conn->slave->width+j] = conn->slave->get(i, j);
      }
    }
  }

  cellstring crealprompt;

  if (commandmode) {
    const char *str = _("crystal> ");
    int max = strlen(str);
    while (1) {
      wchar_t c;
      int s=mbtowc(&c, str, max);
      if (s<=0)
	break;
      crealprompt += cell_t(c);
      str += s;
      max -= s;
    }
  } else {
    for (int i=0;i<grid.col;i++) {
      crealprompt += grid.get(grid.row, i);
    }

    if (grid.cstoredprompt.length() && !crealprompt.length()) {
      for (int i=0;i<grid.cstoredprompt.length();i++) {
	crealprompt += grid.cstoredprompt[i];
      }
    }
  }

  int prlen = crealprompt.length();
  
  if (prlen>(tty.WIDTH-20))
    prlen = (tty.WIDTH-20);

  size_t wid = tty.WIDTH-5;
  if (conn->cursor + prlen>=wid) {
    scroll = conn->cursor-wid+1+prlen;
    a += scroll;
    alen -= scroll;
  }
  wid = tty.WIDTH-1;
  
  int i;
  int pl = crealprompt.length();
  if (pl > (tty.WIDTH-20))
    pl = tty.WIDTH-20;

  for (i=0;i<pl;i++)
    tty.wantbuffer[tty.HEIGHT][i] = crealprompt[i];
  
  wid -= prlen;
  if (!(conn->telnet && conn->telnet->allstars) || commandmode) {
    while (alen && wid) {
      if (real_wcwidth(*a)<=wid) {
	tty.wantbuffer[tty.HEIGHT][i++] = cell_t(*a);
	if (real_wcwidth(*a)==2)
	  tty.wantbuffer[tty.HEIGHT][i++] = cell_t(-*a);
      } else
	break;
      wid -= real_wcwidth(*a);
      a++;
      alen--;
    }
  }
  while (wid) {
    tty.wantbuffer[tty.HEIGHT][i++] = blank;
    wid--;
  }
  if (alen) {
    tty.wantbuffer[tty.HEIGHT][i++] = cell_t('>');
  } else
    tty.wantbuffer[tty.HEIGHT][i++] = cell_t(' ');

  tty.show_want();

  int rcol=col;

  for (int i=0;i<col;i++) {
    if (real_wcwidth(txt[i])==2)
      rcol++;
  }

  if ((conn->telnet && conn->telnet->allstars) && !commandmode) 
    rcol = 0;

  printf("\033[%i;%if", tty.HEIGHT+1, rcol-scroll+1+prlen);
  grid.changed = 0;

  fflush(stdout);
}

