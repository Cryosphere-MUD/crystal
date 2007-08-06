#define TELCMDS
#define TELOPTS
#include <arpa/telnet.h>
#undef TELCMDS
#undef TELOPTS

#include "Socket.h"
#include "telnet.h"
#include "io.h"

#include <iconv.h>
#include <errno.h>

#define TELOPT_MXP 0x5b
#define TELOPT_MPLEX  0x70
#define MPLEX_SELECT  0x71
#define MPLEX_HIDE    0x72
#define MPLEX_SHOW    0x73
#define MPLEX_SETSIZE 0x74

#undef NEGOTIATE_MXP

extern mterm tty;

std::string nam(int i);

void sendwinsize(conn_t *conn) {
  if (!conn->telnet)
    return;

  if (!conn->telnet->s)
    return;

  if (!conn->telnet->do_naws) 
    return;

  std::string str;
  
  int wide = tty.WIDTH;
  int high = tty.HEIGHT+1;

  str += (wide >> 8);
  str += (wide & 0xff);
  str += (high) >> 8;
  str += (high) & 0xff;

  conn->telnet->subneg_send(TELOPT_NAWS, str);
}

std::string nam(int s) {
  if (s > xEOF)
    return telcmds[s - xEOF];
  if (s < NTELOPTS)
    return telopts[s];
  char q[4];
  sprintf(q, "%02x", s);
  return q;
}

void telnet_state::reply(int a, int b, int c) {
  char buf[5];
  buf[0] = a;
  buf[1] = b;
  buf[2] = c;
  s->write(buf, 3);
  debug_fprintf((stderr, "SEND %s %s %s\n", nam(a).c_str(), nam(b).c_str(), nam(c).c_str()));
}

int flag_table[] =
{
  0x2423, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
  0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x2736,
  0x25b8, 0x25c2, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8,
  0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc,
};

void decode(conn_t *conn, grid_t *grid, int ch) {
  int pass = (ch >= 0x20 && ch <= 0x7f) || ch == 033 || ch == '\n' ||
             ch == '\r' || ch == 0;

  if (pass || conn->mud_cset=="ISO-8859-1") {
    grid->wterminal(ch);
    return;
  }

  static iconv_t i=0;
  static std::string cur_cset = "";

  if (conn->mud_cset == "CP437" && ch <= 0x20) {
    grid->wterminal(flag_table[ch]);
    return;
  }

  static char ibuf[10] = {0};
  char *in=ibuf;
  static size_t inb = 0;

  if (!i || cur_cset != conn->mud_cset) {
    if (i)
      iconv_close(i);
    i = iconv_open("WCHAR_T", conn->mud_cset.c_str());
    cur_cset = conn->mud_cset;
    ibuf[0] = 0;
    inb = 0;
  }

  if (!i) {
    grid->wterminal('?');
  }
    
  ibuf[inb++] = ch;
  if (inb>9) inb = 9;
  
  char obuf[5];
  char *out=obuf;
  size_t outb = 4;
  
  if (iconv(i, &in, &inb, &out, &outb)==(size_t)-1) {
    if (errno==EILSEQ) {
      for (size_t id=0;id<inb;id++) {
	grid->wterminal(0xfffd);
      }
      inb = 0;
      iconv(i, NULL, NULL, NULL, NULL);
    }
  }
  
  if (outb == 0) {
    grid->wterminal(*((wchar_t*)obuf));
  }
  return;
}

void telnet_state::decompress(conn_t *conn, unsigned char *bytes, size_t len) {
#ifdef MCCP
  mudcompress_receive(mc, (const char *)bytes, len);
  while (mudcompress_pending(mc)) {
    char buf[2048];
    int got = mudcompress_get(mc, buf, 2048);
    for (int i=0;i<got;i++) {
      conn->telnet->tstack(conn, (unsigned char)buf[i]);
    }
  }
  if (const char *st=mudcompress_response(mc)) {
    s->write(st, strlen(st));
  }
#else
  for (int i=0;i<len;i++)
    conn->telnet->tstack(conn, bytes[i]);
#endif
}

void telnet_state::tstack(conn_t *conn, int ch) {
 
  if (mode == IAC) {
    debug_fprintf((stderr, "%s ", nam(ch).c_str()));

    if (ch == EOR || ch == GA) {
      will_eor = 1;
      have_prompt(conn->grid);
    }
    if (ch == NOP) {
      mode = 0;
      debug_fprintf((stderr, "\n"));
      return;
    }
    if (ch == IAC) {
      if (subneg_type)
	subneg_data += ch;
      else
	decode(conn, conn->cur_grid, 0xff);
      mode = 0;
      debug_fprintf((stderr, "\n"));
      return;
    }
    if (ch == WILL) {
      mode = WILL;
      return;
    }
    if (ch == WONT) {
      mode = WONT;
      return;
    }
    if (ch == DO) {
      mode = DO;
      return;
    }
    if (ch == DONT) {
      mode = DONT;
      return;
    }
    if (ch == SB) {
      mode = SB;
      return;
    }
    if (ch == SE) {
      if (subneg_type == TELOPT_TTYPE) {
	std::string str;
	str += (char)TELQUAL_IS;
	str += (conn->mud_cset == "UTF-8"?"ucryotel":"cryotel");
	subneg_send(TELOPT_TTYPE, str);
      }
      if (subneg_type == TELOPT_MPLEX) {
	if (subneg_data.length()==2) {
	  if (subneg_data[0]==MPLEX_SELECT) {
	    conn->cur_grid = subneg_data[1] ? conn->slave : conn->grid;
	  }
	  if (subneg_data[0]==MPLEX_HIDE) {
	    if (subneg_data[1]==1 && conn->slave) {
	      conn->slave->visible = 0;
	      conn->slave->changed = 1;
	      conn->grid->changed = 1;
	    }
	  }
	  if (subneg_data[0]==MPLEX_SHOW) {
	    if (subneg_data[1]==1 && conn->slave) {
	      conn->slave->visible = 1;
	      conn->slave->changed = 1;
	      conn->grid->changed = 1;
	    }
	  }
	}
	if (subneg_data.length()==6 && subneg_data[0]==MPLEX_SETSIZE) {
	  if (subneg_data[1]==1 && conn->slave) {
	    conn->slave->width  = (subneg_data[2] << 8) | (subneg_data[3]);
	    conn->slave->height = (subneg_data[4] << 8) | (subneg_data[5]);
	    conn->slave->changed = 1;
	  }
	}
      }
      subneg_type = 0;
      mode = 0;
      debug_fprintf((stderr, "\n"));
      return;
    }
    mode = 0;
    debug_fprintf((stderr, "\n"));
    return;
  }

  if (mode == SB) {
    debug_fprintf((stderr, "%s ", nam(ch).c_str()));
    subneg_data = "";
    subneg_type = ch;
    subneg_data = "";
    mode = 0;
    debug_fprintf((stderr, "\n"));
    return;
  }

  if (mode == WILL) {
    debug_fprintf((stderr, "%s ", nam(ch).c_str()));
    if (ch == TELOPT_MPLEX) {
      reply (IAC, DO, TELOPT_MPLEX);
      mode = 0;
      debug_fprintf((stderr, "\n"));
      return;
    }

    if (ch == TELOPT_ECHO) {
      allstars = 1;
      will_echo = 1;
      mode = 0;
      reply(IAC, DO, TELOPT_ECHO);
      debug_fprintf((stderr, "\n"));
      return;
    }

#ifdef WILL_LINEMODE
    if (!rcvd_iac)
      reply(IAC, WILL, TELOPT_LINEMODE);
#endif
    rcvd_iac = 1;

    if (ch == TELOPT_EOR) {
      debug_fprintf((stderr, "\n"));
      if (!will_eor) {
	reply ( IAC, DO, TELOPT_EOR );
	will_eor = 1;
      }
      mode = 0;
      return;
    }

    if (ch == TELOPT_SGA) {
      reply (IAC, DO, TELOPT_SGA);
      mode = 0;
      charmode = 1;
      return;
    }
#ifdef NEGOTIATE_MXP
    if (ch == TELOPT_MXP) {
      reply (IAC, DO, TELOPT_MXP);
      mode = 0;
      return;
    }
#endif
    if (ch == TELOPT_BINARY) {
      reply (IAC, DO, TELOPT_BINARY);
      mode = 0;
      return;
    }

    reply ( IAC, DONT, ch);

    mode = 0;
    debug_fprintf((stderr, "\n"));
    return;
  }

  if (mode == WONT) {
    debug_fprintf((stderr, "%s ", nam(ch).c_str()));
    if (ch == TELOPT_ECHO) {
      allstars = 0;
      will_echo = 0;
      reply(IAC, DONT, TELOPT_ECHO);
    } else {
#ifdef WILL_LINEMODE
      if (!rcvd_iac)
	reply(IAC, WILL, TELOPT_LINEMODE);
#endif
      rcvd_iac = 1;
    }

    mode = 0;
    debug_fprintf((stderr, "\n"));
    return;
  }

  if (mode == DO) {
    debug_fprintf((stderr, "%s ", nam(ch).c_str()));

    if (ch == TELOPT_LINEMODE) {
      charmode = 0;
      mode = 0;
    }

#ifdef WILL_LINEMODE
    if (!rcvd_iac)
      reply(IAC, WILL, TELOPT_LINEMODE);
#endif
    rcvd_iac = 1;

    if (ch == TELOPT_SGA) {
      reply (IAC, WILL, TELOPT_SGA);
      mode = 0;
      return;
    }
    
    if (ch == TELOPT_NAWS) {
      debug_fprintf((stderr, "\n"));
      
      if (!do_naws) {
	reply( IAC, WILL, TELOPT_NAWS);
	do_naws = 1;
	sendwinsize(conn);
      }

      mode = 0;
      return;
    }
    if (ch == TELOPT_TTYPE) {
      debug_fprintf((stderr, "\n"));
      if (!will_ttype) {
	reply ( IAC, WILL, TELOPT_TTYPE );
	will_ttype = 1;
      }
      mode = 0;
      return;
    }

    if (ch == TELOPT_BINARY) {
      reply (IAC, WILL, TELOPT_BINARY);
      mode = 0;
      return;
    }

    debug_fprintf((stderr, "\n"));

    reply (IAC, WONT, ch);

    mode = 0;
    return;
  }

  if (mode == DONT) {
    debug_fprintf((stderr, "%s ", nam(ch).c_str()));
    debug_fprintf((stderr, "\n"));

#ifdef WILL_LINEMODE
    if (!rcvd_iac)
      reply(IAC, WILL, TELOPT_LINEMODE);
#endif
    rcvd_iac = 1;
#ifdef WILL_LINEMODE
    if (ch == TELOPT_LINEMODE)
      charmode = 1;
#endif
    mode = 0;
    return;
  }

  if (ch == IAC) {
    debug_fprintf((stderr, "RECV %s ", nam(ch).c_str()));
    mode = IAC;
    return;
  }

  if (!subneg_type) {
    decode(conn, conn->cur_grid, ch);
  } else {
    debug_fprintf((stderr, "%c", ch));
    subneg_data += ch;
  }
}

void telnet_state::send(const std::string &proper) {
  if (!s)
    return;
  std::string p2;
  const unsigned char *b = (const unsigned char *)proper.c_str();
  while (*b) {
    if (*b==IAC) p2 += IAC;
    p2 += *b;
    b++;
  }
  s->write(p2.c_str(), p2.length());
}

void telnet_state::subneg_send(int subneg, const std::string &proper)
{
  std::string s2;
  s2 += (unsigned char)IAC;
  s2 += (unsigned char)SB;
  s2 += (unsigned char)subneg;

  const unsigned char *b = (const unsigned char *)proper.c_str();
  int l = proper.length();
  while (l) {
    if (*b==IAC) s2 += IAC;
    s2 += *b;
    b++;
    l--;
  }

  s2 += (unsigned char)IAC;
  s2 += (unsigned char)SE;

  s->write(s2.c_str(), s2.length());
}
