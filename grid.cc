/*
 * Crystal Mud Client
 * Copyright (C) 2002-2010 Abigail Brady
 * Copyright (C) 2002 Owen Cliffe
 * Copyright (C) 2004 Stuart Brady
 * Copyright (C) 2004 Paul Lettington
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

#include "grid.h"
#include "common.h"

#include <wchar.h>
#include <ctype.h>

extern mterm tty;

enum {
  TITLEBAR = 256,
  TITLEBAR2 
};

void grid_t::show_batch(const cellstring &batch) {
  for (int i=0;i<batch.length();i++) {
    place(&batch[i]);
  }
}

void grid_t::place(const cell_t *ri) {
    cell_t fi = *ri;
    cell_t *i = &fi;

    if (conn->lp_prompts) {
      if (lastprompt) {
	cstoredprompt.erase();
	for (int i=0;i<col;i++) {
	  cstoredprompt += get(row, i);
	}
	col = 0;
	eraseline(0);
	lastprompt = 0;
      }
    }

    for (int i=0;i<nlw;i++) {
      newline();
    }
    nlw = 0;
    
    if (i->ch == '\r') {
      return;
    }
    if (i->ch == '\t') {
      set(row, col, myblank());
      col++;
      while ((col & 7) && col < tty.WIDTH) {
	set(row, col, myblank());
	col++;
      }
      return;
    }
    
    if (col >= tty.WIDTH || i->ch == '\n') {
      newline();
      if (i->ch == '\n')
	return;
    }
    
    if (((i->ch < 0x20) || (i->ch >= 0x80 && i->ch <= 0x9f)))
      i->ch = 0x241b;
    
    set(row, col, *i);
    col++;
    if (wcwidth(i->ch)==2) {
      set(row, col, cell_t(-i->ch));
      col++;
    } 
}

std::string q(int ch) {
  if (ch==ESC) return "ESC";
  if (ch==CR) return "CR";
  if (ch==CSI) return "CSI";
  if (ch==BS) return "BS";
  if (ch==LF) return "LF";
  if (ch==BEL) return "BEL";
  if (ch==0) return "NUL";
  char a[2] = { ch, 0 };
  return a;
}

void grid_t::wterminal(wchar_t ch) {
  //  fprintf(stderr, "mode: %3s, got:%3s.\n", q(mode).c_str(), q(ch).c_str());

  if (mode==0) {
    switch (ch) {
    case 0:
      return;

    case CR:
      col = 0;
      changed = 1;
      return;

    case BS:
      col--;
      if (col < 0) col = 0;
      changed = 1;
      return;

    case BEL:
      tty.beep();
      return;

    case SI:
      scs = 1;
      return;

    case SO:
      scs = 0;
      return;

    case ESC:
      mode = ESC;
      par = -1;
      {
      std::list<int> q;
      pars = q;
      }
      return;

    case CSI:
      mode = CSI;
      par = -1;
      {
      std::list<int> q;
      pars = q;
      }
      return;

    default:
      
      if (ch >= 0x80 && ch <= 0x9f)
	ch = '?';
      
      if (ch == LF || ch == FF || ch == VT) {
	ch = LF;
      }
      
      if (hidden) ch = ' ';

      int scs = this->scs;
      if (scs && (ch < '`' || ch > '~')) {
	scs = 0;
      }
	
      cell_t a = cell_t(ch, inten, forecol, backcol, scs, ul, it, fr, os, inv);
      place(&a);
      
      //      grid_changed = 1;
      //      buffer_changed = 1;
      changed = 1;
    }
    return;
  }
  
  if (mode == ESC) {
    if (ch == '[') {
      mode = CSI;
      return;
    }
    if (ch == ']') {
      mode = ']';
      return;
    }
    if (ch == '(') {
      mode = '(';
      return;
    }
    if (ch == '%') {
      mode = '%';
      return;
    }
    if (ch == '-') {
      mode = '-';
      return;
    }
    if (ch >= 48 && ch <= 126) {
      mode = 0;
      return;
    }
  }

  if (mode == '%') {
    if (ch == '@')
      conn->mud_cset = "ISO-8859-1";
    
    if (ch == 'G')
      conn->mud_cset = "UTF-8";

    mode = 0;

    return;
  }

  if (mode == '-') {
    if (ch == 'A')
      conn->mud_cset = "ISO-8859-1";
    if (ch == 'B')
      conn->mud_cset = "ISO-8859-2";

    mode = 0;
    return;
  }

  if (mode == '(') {
    mode = 0;
    if (ch == 'B') {
      scs = 0;
    }
    if (ch == '0') {
      scs = 1;
    }  
    return;
  }

  if (mode == ']') {
    if (ch != '0')
      mode = 0;
    else
      mode = TITLEBAR;
    return;
  }

  if (mode == TITLEBAR) {
    if (ch != ';')
      mode = 0;
    else {
      mode = TITLEBAR2;
      title = "";
    }
    return;
  }

  if (mode == TITLEBAR2) {
    if (ch != '\a' && ch != ST) {
      title += ch;
    } else {
      tty.title(_("%s - Crystal"), title.c_str());
      mode = 0;
    }
    return;
  }

  if (mode == CSI) {

    if (isdigit(ch)) {
      if (par == -1) 
	par = (ch - '0');
      else
	par = (par * 10) + (ch - '0');
      return;
    }

    if (ch == ';') {
      pars.insert(pars.end(), par);
      par = -1;
      return;
    }

    if (par!=-1)
      pars.insert(pars.end(), par);

    if (ch=='A') { /* CUU - CUrsor Up */
      if (pars.begin() != pars.end())
       row-=*pars.begin();
      else
       row--;
      if (row<0) row = 0;
      mode = 0;
      changed = 1;
      return;
    }

    if (ch=='H') {
      if (conn->grid != this) {
	row = 0;
	col = 0;
	mode = 0;
	changed = 1;
      }
      mode = 0;
      return;
    }

    if (ch=='D') { /* CUL - cursor left */
      if (pars.begin() != pars.end())
       col-=*pars.begin();
      else
       col--;
      if (col<0) col = 0;
      mode = 0;
      changed = 1;
      return;
    }

    if (ch=='D') { /* CUR - cursor right */
      if (pars.begin() != pars.end())
	col+=*pars.begin();
      else
	col++;
      if (col>=tty.WIDTH) col = tty.WIDTH-1;
      mode = 0;
      changed = 1;
      return;
    }

    if (ch=='K') {  /* EL - Erase in Line */
      if (pars.begin() == pars.end())
	pars.insert(pars.end(), 0);
      
      /* 0 - default - cursor to end of line */
      /* 1 -           start to cursor (including) */
      /* 2 -           all of line */

      par = *pars.begin();
      if (par == 0)
	eraseline(col);
      else if (par == 2)
	eraseline(0);
    }

    if (ch=='m') { /* SGR - Select Graphics Rendition */
      if (pars.begin() == pars.end()) {
	pars.insert(pars.end(), 0);
      }

      std::list<int>::iterator parit = pars.begin();
      while (parit != pars.end()) {
	int parm = *parit;
	parit++;
	switch (parm) {
	case 0:
	  inten = I_NORM;
	  forecol = deffc;
	  backcol = defbc;
	  ul = 0;
	  it = 0;
	  fr = 0;
	  inv = 0;
	  hidden = 0;
	  os = 0;
	  break;
	case 1:
	  inten = I_BOLD;
	  break;
	case 2:
	  inten = I_DIM;
	  break;
	case 3:
	  it = 1;
	  fr = 0;
	  break;
	case 4:
	  ul = 1;
	  break;
	case 7:
	  inv = 1;
	  break;
	case 8:
	  hidden = 1;
	  break;
	case 9:
	  os = 1;
	  break;
	case 20:
	  fr = 1;
	  it = 0;
	  break;
	case 22:
	  inten = I_NORM;
	  break;
	case 23:
	  fr = it = 0;
	  break;
	case 24:
	  ul = 0;
	  break;
	case 27:
	  inv = 0;
	  break;
	case 28:
	  hidden = 0;
	  break;
	case 29:
	  os = 0;
	  break;
	case 30 ... 37:
	  forecol = parm - 30;
	  break;
	case 39:
	  forecol = deffc;
	  break;
	case 38:
	  if (parit != pars.end()) {
	    if (*parit == 5) {
	      parit++;
	      if (parit != pars.end()) {
		int c = *parit;
		if (c >= 0 && c <= 7)
		  forecol = c;
		else if (c >= 8 && c <= 15)
		  forecol = c&7;
		else {
		  forecol = c;
		}
		parit++;
		continue;
	      }
	    }
	  }
	  break;
	case 40 ... 47:
	  backcol = parm - 40;
	  break;
	case 49:
	  backcol = defbc;
	  break;
	case 48:
	  if (parit != pars.end()) {
	    if (*parit == 5) {
	      parit++;
	      if (parit != pars.end()) {
		int c = *parit;
		if (c >= 0 && c <= 7)
		  backcol = c;
		else if (c >= 8 && c <= 15)
		  backcol = c&7;
		else {
		  backcol = c;
		}
		parit++;
		continue;
	      }
	    }
	  }
	  break;
	}
      }

      mode = 0;
      par = -1;
      std::list<int> q; pars = q;
      return;
    }

    if (ch >=0x30 && ch <= 0x7e) {
      mode = 0;
      par = -1;
      std::list<int> q; pars = q;
    }

    return;
  }
}

int info_to_stderr = 1;

void infof(grid_t *g, const char *fmt, ...) {
  char buf[10000];
  va_list ap;
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  va_end(ap);
  info(g, buf);
}

void infof(grid_t *g, FILE *f, const char *fmt, ...) {
  char buf[10000];
  va_list ap;
  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  va_end(ap);
  info(g, buf);
}

void infoc(grid_t *grid, wchar_t w) {
  if (info_to_stderr) {
    fprintf(stderr, "%lc", w);
  }

  static my_wstring isf = L"";

  isf += w;

  if (w=='\n') {
    cellstring q;
    int c = -1;
    int p = grid->lastprompt;
    grid->lastprompt = 0;
    if (grid->col) {
      for (int i=0;i<grid->get_len(grid->row);i++) {
	q += grid->get(grid->row, i);
      }
      c = grid->col;
      grid->col = 0;
      grid->eraseline(0);
    }
    for (size_t i=0;i<isf.length();i++) {
      cell_t c = cell_t(isf[i]);
      grid->place(&c);
    }
    
    if (c != -1) {
      for (int i=0;i<q.length();i++)
	grid->place(&q[i]);
      c = grid->col;
    }
    grid->lastprompt = p;
    grid->changed = 1;
    isf = L"";
    return;
  }
}

void info(grid_t *grid, const char *str) {  
  int max = strlen(str);
  while (1) {
    wchar_t c;
    int s=mbtowc(&c, str, max);
    if (s<=0)
      return;
    infoc(grid, c);
    str += s;
    max -= s;
  }
  
}

void info(grid_t *grid, const wchar_t *w)
{
  while (*w) {
    infoc(grid, *w);
    w++;
  }
}

void info(grid_t *grid, const my_wstring &str) {  
  for (size_t i=0;i<str.length();i++) {
    infoc(grid, str[i]);
  }
}

bool grid_t::file_dump(const char * file) {
  FILE *dumpfile = fopen(file,"a");
  if(NULL==dumpfile){
    infof(this, _("/// couldn't open '%s' to dump to\n"), file);
    return 0;
  }
  infof(this, _("/// dumping scroll history to '%s'\n"), file);
  
  time_t cur_time = time(NULL);
  std::string ctime_str = ctime(&cur_time);
  ctime_str = ctime_str.substr(0,ctime_str.length()-1);
    
  fprintf(dumpfile,_("---=== Dump generated on %s by crystal ===---\n"),
	   ctime_str.c_str());
  for(int i = first;i<row;i++){
    for(int j=0;j<get_len(i);j++)
      fprintf(dumpfile,"%lc",get(i,j).ch);
    fprintf(dumpfile,"\n");
  }
  fprintf(dumpfile,_("---=== End of dump ===---\n"));
  fclose(dumpfile);
  return true;
}

