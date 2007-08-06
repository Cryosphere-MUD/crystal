#ifndef TELNET_H
#define TELNET_H

#include <arpa/telnet.h>

#include "common.h"
#include "crystal.h"
#include "grid.h"
#include "config.h"

#ifdef HAVE_ZLIB
#define MCCP
#endif

#ifdef MCCP
#include "mccpDecompress.h"
#endif

struct telnet_state {
  Socket *s;

  int will_eor;
  int allstars;
  int will_echo;
  int charmode;
  int rcvd_iac;
  int mode;
  int subneg_type;
  int will_ttype;
  std::string subneg_data;
  int do_naws;
#ifdef MCCP
  mc_state *mc;
#endif
  telnet_state(Socket *sock) : 
    s(sock),
    will_eor(0),
    allstars(0),
    will_echo(0),
    charmode(0),
    rcvd_iac(0),
    mode(0),
    subneg_type(0),
    will_ttype(0),
    subneg_data(""),
    do_naws(0)
#ifdef MCCP
    , mc(0)
#endif
{
#ifdef MCCP
    mc = mudcompress_new();
#endif
  }
  
  ~telnet_state() {
#ifdef MCCP
    if (mc) {
      mudcompress_delete(mc);
      mc = 0;
    }
#endif
    if (s) {
      delete s;
      s = 0;
    }
  }

  void tstack(conn_t *conn, int ch);
  void decompress(conn_t *, unsigned char *, size_t);

  void send(const std::string &s);

  void reply(int a, int b, int c);

  void subneg_send(int neg, const std::string &data);
};

void sendwinsize(conn_t *);

#endif
