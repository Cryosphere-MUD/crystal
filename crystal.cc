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

/*
 *
 *   stdin -> charset filter -> esc seq interpreter -> keycode interpreter -> 
 *                            cmd interpreter
 *
 *   read or sslread -> telnet decoder -> charset filter -> tty handler
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <term.h>
#include <curses.h>

#include <locale.h>
#include <wchar.h>
#include <langinfo.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>

#include <netinet/in.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <iconv.h>

#include <list>
#include <vector>
#include <map>
#include <string>
#include <set>

#include <stdarg.h>
#include <ctype.h>

#undef lines
#undef newline
#undef grid

#include "Socket.h"
#include "url.h"

#include "config.h"

#undef SCROLL

//FILE *logfile = 0;

class conn_t;
class grid_t;

#include "io.h"
#include "grid.h"
#include "telnet.h"
#include "crystal.h"
#include "scripting.h"
#include "commands.h"
#include "commandeditor.h"

mterm tty;

extern "C" int mk_wcwidth(wchar_t ucs);

size_t real_wcwidth(wchar_t u) {
  int wid = wcwidth(u);
  if (wid>0) {
    return wid;
  }
  wid = mk_wcwidth(u);
  if (wid>0) {
    return wid;
  }
  wid = 1;
  return 1;
}

const struct cell_t blank(' ');
const struct cell_t blank2('\0');

conn_t::conn_t(grid_t *gr) {
  never_echo = 0;
  quit = 0;

  nofuture = true;

  addrs = 0;
  addr_i = 0;

  cur_grid = grid = gr;
  slave = new grid_t(NULL);
  
  slave->defbc = 0;
  slave->backcol = 0;
  slave->visible = 0;
  
  telnet = 0;

  buffer = L"";
  cursor = 0;
  
  cutbuffer = L"";
  logfile = 0;

  lp_prompts = 1;
  mud_cset = "ISO-8859-1";

  commandmode = false;
  hardscroll = 0;

  gr->set_conn(this);
  if (slave)
    slave->set_conn(this);
}

conn_t::~conn_t() {
  if (slave)
    delete slave;
}

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

void conn_t::dofindnext()
{
  for (int i=hardscroll;i<grid->row;i++) {
    my_wstring s;
    for (int j=0;j<grid->get_len(i);j++) {
      s += grid->get(i, j).ch;
    }
    
    std::set<my_wstring>::iterator it;
    for (it=hl_matches.begin();it!=hl_matches.end();it++) {
      size_t l = s.find(*it);
      while (l != std::string::npos) {
	for (size_t j=0;j<it->length();j++) {
	  hardscroll = i+1;
	  if (hardscroll>(grid->row-tty.HEIGHT))
	    hardscroll = 0;
	  grid->changed = 1;
	  return;
	}
	l = s.find(*it, l+1);
      }
    }
  }
  hardscroll = 0;
  grid->changed = 1;
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

void conn_t::dotoggleslave() {
  if (slave) {
    slave->visible = !slave->visible;
    slave->changed = 1;
  }
}

void conn_t::triggerfn(const char *fn) {
  std::string st = "fn.";
  st += fn;
  st += "\r\n";

  if (telnet)
    telnet->send(st);
}


void conn_t::doscrollstart() {
  if (grid->row < tty.HEIGHT)
    return;
  hardscroll = 1;
  grid->changed = 1;
}

void conn_t::doscrollend() {
  hardscroll = 0;
  grid->changed = 1;
}

void conn_t::dorefresh() {
  grid->changed=1;
  tty.bad_have = 1;
}

void conn_t::dosuspend() {
  kill(0, SIGTSTP);

  struct termios ti;
  cfmakeraw(&ti);
  tcsetattr(0, TCSADRAIN, &ti);
  
  tty.grabwinsize();

  tty.bad_have = 1;
  grid->changed = 1;
  tty.show_want();
  if (tty.titleset) {
    tty.titleset = 0;
    tty.title("%s", tty.curtitle.c_str());
  }
}

void conn_t::doenter()
{
  conn_t *conn= this;

  if (commandmode) {
    my_wstring s = conn->buffer;
    conn->doclearline();

    if (s.length()==0) {
      commandmode = 0;
      return;
    }

    chist()->insert(s);

    commandmode = 0;

    conn->grid->info(_("crystal> "));
    conn->grid->info(s);
    conn->grid->info("\n");

    docommand(conn, s);

    return;
  }
  
  my_wstring wproper = L"";
  std::string proper = "";
  
  for (size_t i=0;i<conn->buffer.length();i++) {
    wchar_t sb = conn->buffer[i];

    wproper += sb;
    
    if (sb < 0x80) {
      proper += sb;
    }  else {

      static iconv_t i=0;
      static std::string cur_cset = "";
    
      if (!i || cur_cset != conn->mud_cset) {
	if (i)
	  iconv_close(i);
	i = iconv_open(conn->mud_cset.c_str(), "WCHAR_T");
	cur_cset = conn->mud_cset;
      }
      wchar_t ic = sb;
      iconv_inptr_t ibuf = (iconv_inptr_t)&ic;
      size_t ilen = 4;
      char o[11];
      char *obuf = o;
      size_t olen = 10;
      iconv(i, &ibuf, &ilen, &obuf, &olen);
      if (olen != 10) {
	*obuf = 0;
	proper += o;
      }
    }
  }

  // If there was stored prompt, echo this into the buffer proper and erase it.
  if (conn->grid->cstoredprompt.length()) {
    for (int i=0;i<conn->grid->cstoredprompt.length();i++) {
      conn->grid->place(&conn->grid->cstoredprompt[i]);
    }
    conn->grid->cstoredprompt.erase();
  }

  conn->grid->lastprompt = 0;

  int toecho = 1, tohistory = 1;

  if (conn->never_echo)
    toecho = 0;

  if (conn->telnet && conn->telnet->will_echo)
    tohistory = toecho = 0;
  
  if (tohistory && wproper.length()) {
    // put the commandline in the history if it isn't a password
    chist()->insert(wproper);
  }
  nofuture = true;

  if (toecho) {
    // echo the commandline if appropriate
    my_wstring a;
    for (int i=0;i<conn->grid->col;i++) {
      a += conn->grid->get(conn->grid->row, i).ch;
    }
    for (size_t i=0;i<wproper.length();i++) {
      cell_t c = cell_t(wproper[i]);
      conn->grid->place(&c);
    }
    conn->grid->wantnewline();
    conn->grid->changed = 1;
  }

  proper += "\r\n";

  // send it
  if (conn->telnet)
    conn->telnet->send(proper);

  conn->doclearline();
}

void conn_t::doscrolldown()
{
  if (hardscroll) {
    hardscroll += 10;
  } 
  if (hardscroll > (grid->row - tty.HEIGHT))
    hardscroll = 0;	      
  grid->changed = 1;
}

void conn_t::doscrollup()
{
  if (grid->row < tty.HEIGHT)
    return;

  if (hardscroll) {
    hardscroll -= 10;
  } else {
    hardscroll = (grid->row - tty.HEIGHT) - 9;
  }
  if (hardscroll < 1)
    hardscroll = 1;
  grid->changed = 1;
}

void conn_t::docommandmode()
{
  if (!commandmode)
    doclearline();
  commandmode = 1;
}

typedef void (conn_t::*keybinding_method_t)();

struct keycommand_t {

  static std::map<std::string, keybinding_method_t> _commands;

public:
  keycommand_t(const char* cmdname, keybinding_method_t method) //: cmdname(cmdname), method(method)
  {
    _commands[cmdname] = method;
  }
  
  static keybinding_method_t findcommand(const char* cmd)
  {
    return _commands[cmd];
  }
};

std::map<std::string, keybinding_method_t> keycommand_t::_commands;

#define DECLARE_COMMAND(str) keycommand_t str ## obj ( # str, &conn_t::do ## str ) ;

DECLARE_COMMAND(commandmode)
DECLARE_COMMAND(backspace)
DECLARE_COMMAND(findnext)
DECLARE_COMMAND(firstchar)
DECLARE_COMMAND(prevchar)
DECLARE_COMMAND(delete)
DECLARE_COMMAND(lastchar)
DECLARE_COMMAND(nextchar)
DECLARE_COMMAND(cutfromhere)
DECLARE_COMMAND(refresh)
DECLARE_COMMAND(nexthistory)
DECLARE_COMMAND(prevhistory)
DECLARE_COMMAND(transpose)
DECLARE_COMMAND(cuttohere)
DECLARE_COMMAND(killword)
DECLARE_COMMAND(paste)
DECLARE_COMMAND(suspend)
DECLARE_COMMAND(prevword)
DECLARE_COMMAND(nextword)
DECLARE_COMMAND(scrollstart)
DECLARE_COMMAND(scrollend)
DECLARE_COMMAND(scrollup)
DECLARE_COMMAND(scrolldown)
DECLARE_COMMAND(toggleslave)
DECLARE_COMMAND(clearline)
DECLARE_COMMAND(enter)

struct keybinding_t {
  const wchar_t *s;
  const char* cmdname;

  keybinding_t(const wchar_t* s, const char* cmdname) : s(s), cmdname(cmdname)
  {
  }

  keybinding_method_t command()
  {
    return keycommand_t::findcommand(cmdname);
  }
};

struct keybinding_t initkeys[34] = {
  keybinding_t( L"c-]",       "commandmode" ),
  keybinding_t( L"backspace", "backspace" ),
  keybinding_t( L"tab",       "findnext" ),
  keybinding_t( L"c-a",       "firstchar" ),
  keybinding_t( L"c-b",       "prevchar" ),
  keybinding_t( L"c-c",       "clearline" ),
  keybinding_t( L"c-d",       "delete" ),
  keybinding_t( L"c-e",       "lastchar" ),
  keybinding_t( L"c-f",       "nextchar" ),
  keybinding_t( L"c-k",       "cutfromhere" ),
  keybinding_t( L"c-l",       "refresh" ),
  keybinding_t( L"c-n",       "nexthistory" ),
  keybinding_t( L"c-p",       "prevhistory" ),
  keybinding_t( L"c-t",       "transpose" ),
  keybinding_t( L"c-u",       "cuttohere" ),
  keybinding_t( L"c-w",       "killword" ),
  keybinding_t( L"c-y",       "paste" ),
  keybinding_t( L"c-z",       "suspend" ),
  keybinding_t( L"m-b",       "prevword" ),
  keybinding_t( L"m-f",       "nextword" ),
  keybinding_t( L"return",    "enter" ),
  keybinding_t( L"up",        "prevhistory" ),
  keybinding_t( L"down",      "nexthistory" ),
  keybinding_t( L"left",      "prevchar" ),
  keybinding_t( L"right",     "nextchar" ),
  keybinding_t( L"m-<",       "scrollstart" ),
  keybinding_t( L"m->",       "scrollend" ),
  keybinding_t( L"home",      "firstchar" ),
  keybinding_t( L"end",       "lastchar" ),
  keybinding_t( L"delete",    "delete" ),
  keybinding_t( L"pagedown",  "scrolldown" ),
  keybinding_t( L"pageup",    "scrollup" ),
  keybinding_t( L"fn.12",     "toggleslave" ),
  keybinding_t( NULL, NULL ),
};

std::map<my_wstring, keybinding_method_t> keys;
std::map<my_wstring, std::string> keystr;

void conn_t::addbinding(const wchar_t* key, const char* cmd)
{
  keybinding_t binding(key, cmd);
  keybinding_method_t handler = binding.command();

  if (!handler) {
    grid->infof(_("/// missing handler for %ls (%s)\n"), key, cmd);
  } else {
    keys[key] = handler;
    keystr[key] = cmd;
  }
}

void conn_t::initbindings()
{
  for (size_t i=0;initkeys[i].s;i++) {
    keybinding_method_t handler = initkeys[i].command();
    if (!handler) {
      grid->infof(_("/// missing handler for %ls (%s)\n"), initkeys[i].s, initkeys[i].cmdname);
    } else {
      keys[initkeys[i].s] = handler;
      keystr[initkeys[i].s] = initkeys[i].cmdname;
    }
  }
}

void conn_t::dispatch_key(const my_wstring &s)
{
  if (s.length()==1) {
    doinsertchar(s[0]);
    return;
  }
  
  if (keys.find(s) != keys.end()) {
    keybinding_method_t handler = keys[s];
    if (handler)
      (this->*handler)();
    else
      grid->infof(_("/// missing handler for %ls\n"), s.c_str());
    return;
  }

  if (s.length()>3 && s[0]=='f' && s[1]=='n' && s[2]=='.') {
    std::string cs = mks(s);
    triggerfn(cs.c_str()+3);
    return;
  }
}

void cmd_bind(conn_t *conn, const cmd_args &arg)
{
  if (arg.size() != 1 && arg.size() != 3) {
    conn->grid->infof(_("/// set [option value]\n"));
    return;
  }

  if (arg.size() == 1) {
    int wid = 0;
    for (std::map<my_wstring, std::string>::const_iterator it = keystr.begin();
	 it != keystr.end();
	 it++) {
      wid = std::max(wid, int(it->first.length()));
    }

    for (std::map<my_wstring, std::string>::const_iterator it = keystr.begin();
	 it != keystr.end();
	 it++) {
      conn->grid->infof("%*ls %s\n", wid, it->first.c_str(), it->second.c_str());
    }
  } else {
    std::string cmd;
    for (int i = 0; i < arg[2].size(); i++)
      cmd += arg[2][i];
    conn->addbinding(arg[1].c_str(), cmd.c_str());
  }
}

int had_winch = 0;

void winch(int) {
  tty.grabwinsize();
  had_winch = 1;
}

void conn_t::connected()
{
  conn_t* conn = this;
  
  conn->grid->infof(_("/// connected\n"));
  
  static int a = 0;
  if (!a) {
    conn->grid->infof(_("/// escape character is '%s'\n"), "^]");
    a = 1;
  }  

  if (conn->ssl)
    tty.title(_("telnets://%s:%i - Crystal"), conn->host.c_str(), conn->port);
  else
    tty.title(_("telnet://%s:%i - Crystal"), conn->host.c_str(), conn->port);
}

bool conn_t::try_addr(const char *host, int port, bool ssl)
{
  conn_t* conn = this;

  if (!conn->addrs)
    return false;

  if (conn->addr_i>=conn->addrs->size()) {
    return false;
  }

  InAddr *addr = conn->addrs->get(conn->addr_i);

  Socket *s2 = new Socket(ssl);
  addr->set_port(port);
  int stat = s2->connect(addr);

  int e = errno;
  if (stat < 0) {
    conn->grid->infof(_("/// unable to connect : %s.\n"), strerror(e));
    delete s2;

    conn->addr_i++;
    return try_addr(host, port, ssl);
  }

  conn->grid->infof(_("/// connecting to %s:%i\n"), addr->tostring().c_str(), port);
  
  if (s2->getpend()==0)
    connected();
  
  if (conn->telnet)
    delete conn->telnet;
  conn->telnet = new telnet_state(s2);
  conn->grid->cstoredprompt.erase();
  return true;
}

void conn_t::connect(const char *host, int port, bool ssl)
{
  /* nuke old stuff */
  delete telnet;
  telnet = 0;

  if (slave) slave->visible = 0;
  if (grid->col) 
    grid->newline();

  grid->infof(_("/// resolving %s\n"), host, port);
  display_buffer();

  if (addrs)
    delete addrs;

  addrs = InAddr::resolv(host);
  if (!addrs) {
    grid->infof(_("/// unknown host.\n"));
    return;
  }
  addr_i = 0;
  host = host;
  port = port;
  ssl = ssl;
  try_addr(host, port, ssl);
}

bool conn_t::file_log(const char *filename)
{
  if (logfile) {
    grid->info(_("/// closing old logfile.\n"));
    fclose(logfile);
    logfile = NULL;
  }
  logfile = fopen(filename, "a");
  if (!logfile) {
    grid->infof(_("/// couldn't open '%s' for appending.\n"), filename);
    return false;
  } else {
    grid->infof(_("/// logging to end of '%s'.\n"), filename);
    return true;
  }
}

void main_loop(conn_t *conn) 
{
  if (!conn->telnet)
    conn->commandmode = 1;

  conn->grid->changed = 1;
  tty.bad_have = 1;

  conn->display_buffer();
  
  while (!conn->quit) {
    fd_set r, e, w;

    int maxfd = 1;

    FD_ZERO(&r);
    FD_ZERO(&e);
    FD_ZERO(&w);

    if (conn->telnet && conn->telnet->s) {
      FD_SET(conn->telnet->s->getfd(),&r);
      FD_SET(conn->telnet->s->getfd(),&e);
      if (conn->telnet->s->getpend()) {
	FD_SET(conn->telnet->s->getfd(),&w);
      }

      maxfd = conn->telnet->s->getfd() + 1;
    }

    FD_SET(0, &r);

    struct timeval tm;
    struct timeval *tmp = &tm;
    if (conn->grid->nlw) {
      tm.tv_usec = 100000;
      tm.tv_sec = 0;
    } else if (scripting::count_timers()) {
      tm.tv_usec = 0;
      tm.tv_sec = 1;
    } else {
      tmp = NULL;
    }

    select(maxfd, &r, &w, &e, tmp);

    scripting::dotimers();

    if (had_winch) {
      sendwinsize(conn);
      had_winch = 0;
      conn->grid->changed = 1;
      tty.bad_have = 1;
    }
    
    for (int i=0;i<conn->grid->nlw;i++) {
      conn->grid->newline();
      conn->grid->changed = 1;
    }
    conn->grid->nlw = 0;

    if (conn->telnet && conn->telnet 
	&& (FD_ISSET(conn->telnet->s->getfd(), &r) || 
	    FD_ISSET(conn->telnet->s->getfd(), &e) ||
	    FD_ISSET(conn->telnet->s->getfd(), &w))) {
      int ok = 0;
      do {
	unsigned char mbuffer[1000];

	int pend = conn->telnet->s->getpend();
	int bts = conn->telnet->s->read((char *)mbuffer, 500);

	if (pend && !conn->telnet->s->getpend() && 
	    !conn->telnet->s->getdead()) {
	  conn->connected();
	}

	ok = 0;
	if (bts == 0 || (bts==-1 && conn->telnet->s->getdead())) {
	  tty.title(_("Disconnected - Crystal"));
	  if (conn->grid->col) {
	    conn->grid->newline();
	  }
	  if (bts==-1) {
	    int e = errno;
	    conn->grid->infof(_("/// connection closed : %s.\n"), strerror(e));
	    if (pend && (conn->addr_i < conn->addrs->size())) {
	      conn->addr_i++;
	      if (conn->try_addr(conn->host.c_str(), conn->port, conn->ssl)) {
		conn->display_buffer();
		conn->grid->changed = 1;
		fflush(stdout);
		continue;
	      }
	    }

	  } else {
	    conn->grid->info(_("/// connection closed by foreign host.\n"));
	  }
	  conn->display_buffer();
	  fflush(stdout);
	  ok = 0;
	  delete conn->telnet;
	  conn->telnet = 0;
	  conn->commandmode = 1;
	  conn->grid->changed = 1;
	  break;
	} else if (bts!=-1) {
	  conn->telnet->decompress(conn, mbuffer, bts);
	  ok = 1;
	}
      } while (ok);
    }

    if (FD_ISSET(0, &r)) {
      
      while (1) {
	int foo;
	ioctl(0, FIONREAD, &foo);
	if(foo) {
	  wchar_t i = tty.getinput();

	  if (conn->telnet && conn->telnet->charmode && !conn->commandmode
	      && i != 0x1d) {
	    std::string p;
	    p += i;
	    conn->telnet->send(p);
	  } else {
	    my_wstring s = tty.convert_input(i);
	    if (s.length())
	      conn->dispatch_key(s);
	  }

	  if (!conn->telnet) {
	    conn->commandmode = 1;
	  }
	  conn->grid->changed = 1;
	} else {
	  break;
	}
      }
    }

    if (!conn->quit) {
      conn->display_buffer();
      fflush(stdout);
    }
  }
}

struct termios oldti;

conn_t* cleanupConn;

void cleanup()
{
  if (cleanupConn) {
    conn_t& conn = *cleanupConn;

    if (conn.logfile) {
      fclose(conn.logfile);
      conn.logfile = 0;
    }
    
    tcsetattr(0, TCSADRAIN, &oldti);
    
    cleanupConn = 0;
  }
}

int main(int argc, char **argv) {
  char *pname = argv[0];

  setlocale(LC_ALL, "");
  const char *codeset = nl_langinfo(CODESET);

  tty.getterm();

  grid_t grid(NULL);
  conn_t conn(&grid);
  grid.set_conn(&conn);
  
  conn.initbindings();
  
  scripting::set_grid(&grid);
  scripting::start();

  if (strcmp(codeset, "UTF-8")==0) {
    tty.utf8 = 1;
    conn.mud_cset = "UTF-8";
  }

  if (argv[1] && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v"))) {
    printf("crystal %s\n", VERSION);
    return 0;
  }

  if (argv[1] && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
    printf("Usage: crystal [-n] <hostname> <port>\n");
    printf("       crystal [-n] telnets://hostname:port/\n");
    printf("Options:\n");
    printf("       -n never echo locally.\n");
    return 0;
  }

  if (argv[1] && strcmp(argv[1], "-n")==0) {
    conn.never_echo = 1;
    argv++;
    argc--;
  }

#ifdef I18N
  bindtextdomain(PACKAGE, LOCALEDIR);
  textdomain(PACKAGE);
#endif
  if (argc>1) {
    url u = url(argv[1]);
    if (argc>2) {
      u.service = argv[2];
    }

    if (!valid_protocol(u.protocol)) {
      fprintf(stderr, _("%s: Bad protocol - '%s'.\n"), pname, u.protocol.c_str());
      exit(1);
    }

    int port = lookup_service(u.service);
    if (port == -1) {
      fprintf(stderr, _("%s: Bad port - '%s'.\n"), pname, u.service.c_str());
      exit(1);
    }

    conn.connect(u.hostname.c_str(), port, u.protocol == "telnets");
    if (!conn.telnet)
      exit(1);

    wchar_t blah[1000];
    if (argc>2)
      swprintf(blah, 1000, L"connect %s %s", argv[1], argv[2]);
    else
      swprintf(blah, 1000, L"connect %s", argv[1]);
    cmdhist.insert(blah);
  }

  info_to_stderr = 0;

  tty.grabwinsize();

  setupterm(NULL, 0, NULL);
  printf("%s", tty.getinfo("enacs", "").c_str());

  struct termios ti;
  tcgetattr(0, &oldti);
  cfmakeraw(&ti);
  tcsetattr(0, TCSADRAIN, &ti);

  cleanupConn = &conn;
  atexit(cleanup);

  signal(SIGWINCH, winch);

  main_loop(&conn);

  cleanup();
  printf("\n");

  return 0;
}
