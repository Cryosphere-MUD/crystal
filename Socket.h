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
