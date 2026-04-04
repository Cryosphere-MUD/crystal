#include <string.h>

#include "scripting.h"
#include "url.h"

size_t count_chars(const std::string &s, char to_count)
{
	size_t count = 0;
	for (auto ch : s)
		if (ch == to_count)
			count++;
	return count;
}

URL::URL(const std::string &url) : has_username(false), has_password(false)
{
	const char *s = url.c_str();
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
