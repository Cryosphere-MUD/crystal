/* 
 * Crystal Mud Client
 * Copyright (C) Abigail Brady, Paul Lettington, Owen Cliffe, Stuart Brady
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

#ifndef SOCKET_H
#define SOCKET_H

#include "config.h"

#include <string>
#include <vector>

#include <sys/socket.h>
#include <netinet/in.h>

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

int lookup_service(const std::string &name);

class InAddr;

class InAddrList {
  std::vector<InAddr*> addrs;
 public:
  InAddrList();
  void add(InAddr *);
  InAddr *get(int i) { return addrs[i]; }
  int size() { return addrs.size(); }
  ~InAddrList();
};

class InAddr {
public:
  static InAddrList *resolv(const char *name);
  static InAddr *create(const struct sockaddr *addr);

  virtual ~InAddr() { };

  virtual void set_port(int port)=0;
  virtual int get_port()=0;

  virtual std::string tostring() = 0;
  virtual sockaddr *addr() = 0;
  virtual socklen_t addr_len() = 0;
  virtual int af() = 0;

  virtual operator sockaddr* () { return addr(); };
};

class Socket {
  int fd;
  bool dead;
  bool pending;
  std::string pendingoutput;
#ifdef HAVE_SSL
  SSL *s;
#endif
  void connected();
  
 public:
  bool getdead() { return dead; }
  bool getpend() { return pending; }
  Socket(bool ssl);
  int connect(InAddr *where);
  int read(char *, int);
  int write(const char *, int);
  void close();
  virtual ~Socket();
  int getfd() { return fd; }
};

struct url {
  std::string protocol;
  std::string hostname;
  std::string service;

  std::string username;
  bool has_username;
  std::string password;
  bool has_password;

  url(const char *);
};

#endif
