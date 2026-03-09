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

#include "commands.h"
#include "crystal.h"
#include "grid.h"

typedef void (conn_t::*keybinding_method_t)();

struct keycommand_t
{

	static std::map<std::string, keybinding_method_t> _commands;

      public:
	keycommand_t(const char *cmdname, keybinding_method_t method) //: cmdname(cmdname), method(method)
	{
		_commands[cmdname] = method;
	}

	static keybinding_method_t findcommand(const char *cmd) { return _commands[cmd]; }
};

std::map<std::string, keybinding_method_t> keycommand_t::_commands;

#define DECLARE_COMMAND(str) keycommand_t str##obj(#str, &conn_t::do##str);

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
DECLARE_COMMAND(toggleoverlay)
DECLARE_COMMAND(clearline)
DECLARE_COMMAND(enter)

struct keybinding_t
{
	const wchar_t *s;
	const char *cmdname;

	keybinding_t(const wchar_t *s, const char *cmdname) : s(s), cmdname(cmdname) {}

	keybinding_method_t command() { return keycommand_t::findcommand(cmdname); }
};

struct keybinding_t initkeys[34] = {
    keybinding_t(L"c-]", "commandmode"),     keybinding_t(L"backspace", "backspace"),
    keybinding_t(L"tab", "findnext"),	     keybinding_t(L"c-a", "firstchar"),
    keybinding_t(L"c-b", "prevchar"),	     keybinding_t(L"c-c", "clearline"),
    keybinding_t(L"c-d", "delete"),	     keybinding_t(L"c-e", "lastchar"),
    keybinding_t(L"c-f", "nextchar"),	     keybinding_t(L"c-k", "cutfromhere"),
    keybinding_t(L"c-l", "refresh"),	     keybinding_t(L"c-n", "nexthistory"),
    keybinding_t(L"c-p", "prevhistory"),     keybinding_t(L"c-t", "transpose"),
    keybinding_t(L"c-u", "cuttohere"),	     keybinding_t(L"c-w", "killword"),
    keybinding_t(L"c-y", "paste"),	     keybinding_t(L"c-z", "suspend"),
    keybinding_t(L"m-b", "prevword"),	     keybinding_t(L"m-f", "nextword"),
    keybinding_t(L"return", "enter"),	     keybinding_t(L"up", "prevhistory"),
    keybinding_t(L"down", "nexthistory"),    keybinding_t(L"left", "prevchar"),
    keybinding_t(L"right", "nextchar"),	     keybinding_t(L"m-<", "scrollstart"),
    keybinding_t(L"m->", "scrollend"),	     keybinding_t(L"home", "firstchar"),
    keybinding_t(L"end", "lastchar"),	     keybinding_t(L"delete", "delete"),
    keybinding_t(L"pagedown", "scrolldown"), keybinding_t(L"pageup", "scrollup"),
    keybinding_t(L"fn.12", "toggleoverlay"), keybinding_t(NULL, NULL),
};

std::map<my_wstring, keybinding_method_t> keys;
std::map<my_wstring, std::string> keystr;

void conn_t::addbinding(const wchar_t *key, const char *cmd)
{
	keybinding_t binding(key, cmd);
	keybinding_method_t handler = binding.command();

	if (!handler)
	{
		grid->infof(_("/// missing handler for %ls (%s)\n"), key, cmd);
	}
	else
	{
		keys[key] = handler;
		keystr[key] = cmd;
	}
}

void conn_t::initbindings()
{
	for (size_t i = 0; initkeys[i].s; i++)
	{
		keybinding_method_t handler = initkeys[i].command();
		if (!handler)
		{
			grid->infof(_("/// missing handler for %ls (%s)\n"), initkeys[i].s, initkeys[i].cmdname);
		}
		else
		{
			keys[initkeys[i].s] = handler;
			keystr[initkeys[i].s] = initkeys[i].cmdname;
		}
	}
}

void conn_t::dispatch_key(const my_wstring &s)
{
	if (s.length() == 1)
	{
		doinsertchar(s[0]);
		return;
	}

	if (keys.find(s) != keys.end())
	{
		keybinding_method_t handler = keys[s];
		if (handler)
			(this->*handler)();
		else
			grid->infof(_("/// missing handler for %ls\n"), s.c_str());
		return;
	}

	if (s.length() > 3 && s[0] == 'f' && s[1] == 'n' && s[2] == '.')
	{
		std::string cs = mks(s);
		triggerfn(cs.c_str() + 3);
		return;
	}
}

void cmd_bind(conn_t *conn, const cmd_args &arg)
{
	if (arg.size() != 1 && arg.size() != 3)
	{
		conn->grid->infof(_("/// set [option value]\n"));
		return;
	}

	if (arg.size() == 1)
	{
		int wid = 0;
		for (std::map<my_wstring, std::string>::const_iterator it = keystr.begin(); it != keystr.end(); it++)
			wid = std::max(wid, int(it->first.length()));

		for (std::map<my_wstring, std::string>::const_iterator it = keystr.begin(); it != keystr.end(); it++)
			conn->grid->infof("%*ls %s\n", wid, it->first.c_str(), it->second.c_str());
	}
	else
	{
		std::string cmd;
		for (int i = 0; i < arg[2].size(); i++)
			cmd += arg[2][i];
		conn->addbinding(arg[1].c_str(), cmd.c_str());
	}
}
