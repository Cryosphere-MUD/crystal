/* 
 * Crystal Mud Client
 * Copyright (C) 1998-2005 Abigail Brady
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

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Socket.h"
#include "crystal.h"
#include "grid.h"
#include "io.h"
#include "scripting.h"
#include "telnet.h"

class InAddr4 : public InAddr
{
	struct sockaddr_in s;

      public:
	InAddr4(const struct in_addr *si)
	{
		s.sin_family = AF_INET;
		s.sin_port = 0;
		s.sin_addr = *si;
	}
	InAddr4(const struct sockaddr_in *si) : s(*si) {}
	virtual ~InAddr4() {}
	void set_port(int port) { s.sin_port = htons(port); }
	int get_port() { return s.sin_port; }
	virtual std::string tostring() { return inet_ntoa(s.sin_addr); }
	sockaddr *addr() { return (sockaddr *)&s; }
	socklen_t addr_len() { return sizeof s; }
	int af() { return AF_INET; };
};

#ifdef AF_INET6
class InAddr6 : public InAddr
{
	struct sockaddr_in6 s;

      public:
	InAddr6(const struct in6_addr *si)
	{
		s.sin6_family = AF_INET6;
		s.sin6_port = 0;
		s.sin6_addr = *si;
	}
	InAddr6(const struct sockaddr_in6 *si) : s(*si) {}
	virtual ~InAddr6(){};
	void set_port(int port) { s.sin6_port = htons(port); };
	int get_port() { return s.sin6_port; }
	virtual std::string tostring()
	{
		char dest[100];
		inet_ntop(AF_INET6, &s.sin6_addr, dest, 100);
		dest[99] = 0;
		return dest;
	}
	sockaddr *addr() { return (sockaddr *)&s; }
	socklen_t addr_len() { return sizeof s; }
	int af() { return AF_INET6; };
};
#endif

InAddrPtr InAddr::create(const struct sockaddr *addr)
{
	if (addr->sa_family == AF_INET)
		return std::make_shared<InAddr4>((const struct sockaddr_in *)addr);
#ifdef AF_INET6
	if (addr->sa_family == AF_INET6)
		return std::make_shared<InAddr6>((const struct sockaddr_in6 *)addr);
#endif
	return nullptr;
}

InAddrListPtr InAddr::resolv(const char *name)
{
#ifdef AF_INET6
	struct hostent *he4 = gethostbyname2(name, AF_INET);
	struct hostent *he6 = gethostbyname2(name, AF_INET6);
#endif
	struct hostent *he = gethostbyname(name);
	if (!he
#ifdef AF_INET6
	    && !he4 && !he6
#endif
	)
		return nullptr;

	auto list = std::make_shared<InAddrList>();
#ifdef AF_INET6
	if (he4 && he4->h_addrtype == AF_INET)
	{
		auto addr = (struct in_addr *)he4->h_addr_list[0];
		if (addr != nullptr)
			list->add(std::make_shared<InAddr4>(addr));
	}
	if (he6 && he6->h_addrtype == AF_INET6)
	{
		auto addr = (struct in6_addr *)he6->h_addr_list[0];
		if (addr != nullptr)
			list->add(std::make_shared<InAddr6>(addr));
	}
#endif
	if (he && he->h_addrtype == AF_INET)
	{
		auto addr = (struct in_addr *)he->h_addr_list[0];
		if (addr != nullptr)
			list->add(std::make_shared<InAddr4>(addr));
	}

	return list;
}

#ifdef HAVE_SSL
class Ctx
{
      public:
	Ctx()
	{
		SSL_load_error_strings();
		SSL_library_init();
		c = SSL_CTX_new(SSLv23_client_method());
	}
	static SSL_CTX *c;
} c;

SSL_CTX *Ctx::c;
#endif

Socket::Socket(bool ssl) : fd(-1), dead(0), pending(0)
{
#ifdef HAVE_SSL
	if (ssl)
		s = SSL_new(Ctx::c);
	else
		s = 0;
#endif
}

void Socket::connected()
{
#ifdef HAVE_SSL
	if (s)
	{
		SSL_set_fd(s, fd);
		SSL_set_verify(s, 0, 0);
		SSL_connect(s);
	}
#endif
}

int Socket::connect(InAddrPtr addr)
{
	fd = socket(addr->af(), SOCK_STREAM, 0);
	if (fd == -1)
		return -1;

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

	int value = ::connect(fd, addr->addr(), addr->addr_len());

	int er = errno;

	if (value == -1 && er == EINPROGRESS)
	{
		dead = false;
		pending = true;
		return true;
	}

	if (value == -1)
	{
		::close(fd);
		errno = er;
		return -1;
	}

	dead = false;
	pending = false;

	connected();

	return true;
}

int Socket::read(char *data, int size)
{
	if (pending)
	{
		int opt = 0;
		socklen_t osz = sizeof(int);
		getsockopt(fd, SOL_SOCKET, SO_ERROR, &opt, &osz);
		pending = 0;
		if (opt != 0)
			dead = 1;
		else
			connected();
		errno = opt;
		return -1;
	}

	write(pendingoutput.data(), pendingoutput.length());
	pendingoutput = "";

	if (dead)
	{
		errno = ECONNRESET;
		return -1;
	}
#ifdef HAVE_SSL
	if (s)
	{
		int rval = SSL_read(s, data, size);
		if (rval == -1 && SSL_get_error(s, rval) == SSL_ERROR_WANT_READ)
		{
			errno = EAGAIN;
			return -1;
		}
		if (rval == -1)
			dead = 1;
		return rval;
	}
#endif
	int rval = ::read(fd, data, size);
	if (rval == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return -1;
		dead = true;
	}
	return rval;
}

int Socket::write(const char *data, int size)
{
	if (pending)
	{
		pendingoutput += std::string(data, size);
		return size;
	}
	if (dead)
	{
		errno = ECONNRESET;
		return -1;
	}
	if (!size)
		return 0;
#ifdef HAVE_SSL
	if (s)
	{
		int rval = SSL_write(s, data, size);
		if (rval == -1 && SSL_get_error(s, rval) == SSL_ERROR_WANT_WRITE)
		{
			errno = EAGAIN;
			return -1;
		}
		if (rval == -1)
			dead = 1;
		return rval;
	}
#endif
	int rval = ::write(fd, data, size);
	if (rval == -1)
		dead = true;
	return rval;
}

Socket::~Socket()
{
#ifdef HAVE_SSL
	if (s)
		SSL_free(s);
#endif
	if (!dead)
	{
		shutdown(fd, 2);
		::close(fd);
	}
}

void Socket::close()
{
	shutdown(fd, 2);
	::close(fd);
	dead = true;
}

int lookup_service(const std::string &name)
{
	char *e = 0;
	int port = strtol(name.c_str(), &e, 10);
	if (!*e)
		return port;

	struct servent *se = getservbyname(name.c_str(), "tcp");
	if (se)
		return ntohs(se->s_port);
	else
		return -1;
}

int count_chars(const char *s, char ch)
{
	int c = 0;
	while (*s)
	{
		if (*s == ch)
			c++;
		s++;
	}
	return c;
};

url::url(const char *s) : has_username(false), has_password(false)
{
	protocol = "telnet";
	service = "telnet";

	if (const char *t = strstr(s, "://"))
	{
		protocol = std::string(s, t - s);
		s = t + strlen("://");
	}

	if (protocol == "stelnet")
		protocol = "telnets";

	if (protocol == "telnets")
		service = "telnets";

	const char *srv = 0, *lasth = 0;

	if (const char *u = strchr(s, '@'))
	{
		printf("We have a username part.\n");
		username = std::string(s, u - s);
		has_username = true;
		if (username.find(':') != std::string::npos)
		{
			password = username.substr(username.find(':') + 1);
			username = username.substr(0, username.find(':'));
			has_password = true;
		}
		s = u + 1;
	}

	if (s[0] == '[')
	{
		// we have a 6 ip address encoded in [numeric] form.
		s++;
		srv = strchr(s, ']');
		if (srv && srv[1] == ':')
		{
			lasth = srv;
			srv = srv + 2;
		}
		else
		{
			srv = 0;
		}
	}
	else if (count_chars(s, ':') >= 2)
	{
		// it is a 6 address with no service at the end - do nothing
	}
	else
	{
		srv = strchr(s, ':');
		if (srv)
		{
			lasth = srv;
			srv++;
		}
	}

	if (lasth)
		hostname = std::string(s, lasth - s);
	else
		hostname = s;

	if (hostname.length() && hostname[hostname.length() - 1] == '/')
		hostname = hostname.substr(0, hostname.length() - 1);

	std::string newhost = scripting::lookup_host(hostname);
	if (newhost.length())
	{
		if (newhost.find(':') != std::string::npos)
		{
			int w = newhost.find(':');
			service = newhost.substr(w + 1);
			hostname = newhost.substr(0, w);
		}
		else
		{
			hostname = newhost;
		}
	}
	if (srv)
	{
		const char *sl = strchr(srv, '/');
		service = sl ? std::string(srv, sl - srv) : std::string(srv);
	}
}
