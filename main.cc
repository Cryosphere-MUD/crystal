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

extern mterm tty;

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

bool had_winch = 0;

int exitValue = 0;

void winch(int) {
  tty.grabwinsize();
  had_winch = 1;
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
  init_commands();
  
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
    printf("Usage: crystal [-n] [-s] <hostname> <port>\n");
    printf("       crystal [-n] telnets://hostname:port/\n");
    printf("Options:\n");
    printf("       -n never echo locally.\n");
    printf("       -s use TLS.\n");
    return 0;
  }

  bool force_tls = false;

  while (argv[1]) {
    if (strcmp(argv[1], "-n")==0) {
      conn.never_echo = 1;
      argv++;
      argc--;
      continue;
    }
    if (strcmp(argv[1], "-s")==0) {
      force_tls = true;
      argv++;
      argc--;
      continue;
    }    
    break;
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
    const char *tlsopt = "";
    if (force_tls)
    {
        u.protocol = "telnets";
        tlsopt = "-s ";
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
      swprintf(blah, 1000, L"connect %s%s %s", tlsopt, argv[1], argv[2]);
    else
      swprintf(blah, 1000, L"connect %s%s", tlsopt, argv[1]);
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

  conn.main_loop();

  cleanup();
  printf("\n");

  return exitValue;
}
