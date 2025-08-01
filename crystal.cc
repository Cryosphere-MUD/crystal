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
  overlay = new grid_t(NULL);
  
  overlay->defbc = 0;
  overlay->backcol = 0;
  overlay->visible = 0;
  
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
  if (overlay)
    overlay->set_conn(this);
}

conn_t::~conn_t() {
  if (overlay)
    delete overlay;
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

void conn_t::dotoggleoverlay() {
  if (overlay) {
    overlay->visible = !overlay->visible;
    overlay->changed = 1;
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

void conn_t::connected()
{
  conn_t* conn = this;
  
  conn->grid->infof(_("/// connected with %s\n"), conn->ssl ? "TLS" : "telnet");
  
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

  auto addr = conn->addrs->get(conn->addr_i);

  auto s2 = std::make_shared<Socket>(ssl);
  addr->set_port(port);
  conn->port = port;

  int stat = s2->connect(addr);

  int e = errno;
  if (stat < 0) {
    conn->grid->infof(_("/// unable to connect : %s.\n"), strerror(e));
    s2.reset();
    conn->addr_i++;
    return try_addr(host, port, ssl);
  }

  conn->grid->infof(_("/// connecting to %s:%i\n"), addr->tostring().c_str(), port);
  
  if (s2->getpend()==0)
    connected();
  
  conn->telnet = std::make_shared<telnet_state>(s2);
  conn->grid->cstoredprompt.erase();
  return true;
}

void conn_t::connect(const char *host, int port, bool ssl)
{
  /* nuke old stuff */
  telnet.reset();

  if (overlay) overlay->visible = 0;
  if (grid->col) 
    grid->newline();

  grid->infof(_("/// resolving %s\n"), host, port);
  display_buffer();

  addrs = InAddr::resolv(host);
  if (!addrs) {
    grid->infof(_("/// unknown host.\n"));
    return;
  }
  addr_i = 0;
  this->host = host;
  this->port = port;
  this->ssl = ssl;
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

bool conn_t::disconnected(int bts, int pend)
{
  conn_t* conn = this;
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
	return true;
      }
    }
    
  } else {
    conn->grid->info(_("/// connection closed by foreign host.\n"));
  }
  conn->display_buffer();
  fflush(stdout);
  conn->telnet.reset();
  conn->commandmode = 1;
  conn->grid->changed = 1;
  return false;
}

void conn_t::main_loop()
{
  conn_t *conn = this;

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

    extern bool had_winch;

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
	  if (conn->disconnected(bts, pend))
	    continue;
	  else {
	    ok = false;
	    break;
	  }
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

