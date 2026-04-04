#include <string.h>

#include "scripting.h"
#include "url.h"

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
}

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
