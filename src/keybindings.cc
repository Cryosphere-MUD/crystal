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
#include "Runtime.h"
#include "grid.h"

#include <iostream>

using KeyBindingMethod = void(Runtime::*)();

struct KeyCommand
{

	static std::map<std::string, KeyBindingMethod> _commands;

      public:
	KeyCommand(const std::string &cmdname, KeyBindingMethod method) //: cmdname(cmdname), method(method)
	{
		_commands[cmdname] = method;
	}

	static KeyBindingMethod findcommand(const std::string &cmd) { return _commands[cmd]; }
};

std::map<std::string, KeyBindingMethod> KeyCommand::_commands;

#define DECLARE_COMMAND(str) KeyCommand str##obj(#str, &Runtime::do##str);

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

struct KeyBinding
{
	const wchar_t *s;
	std::string cmdname;

	KeyBinding(const wchar_t *s, const std::string &cmdname) : s(s), cmdname(cmdname) {}

	KeyBindingMethod command() const { return KeyCommand::findcommand(cmdname); }
};

struct KeyBinding initkeys[] = {
    KeyBinding(L"c-]", "commandmode"),     KeyBinding(L"backspace", "backspace"),
    KeyBinding(L"tab", "findnext"),	     KeyBinding(L"c-a", "firstchar"),
    KeyBinding(L"c-b", "prevchar"),	     KeyBinding(L"c-c", "clearline"),
    KeyBinding(L"c-d", "delete"),	     KeyBinding(L"c-e", "lastchar"),
    KeyBinding(L"c-f", "nextchar"),	     KeyBinding(L"c-k", "cutfromhere"),
    KeyBinding(L"c-l", "refresh"),	     KeyBinding(L"c-n", "nexthistory"),
    KeyBinding(L"c-p", "prevhistory"),     KeyBinding(L"c-t", "transpose"),
    KeyBinding(L"c-u", "cuttohere"),	     KeyBinding(L"c-w", "killword"),
    KeyBinding(L"c-y", "paste"),	     KeyBinding(L"c-z", "suspend"),
    KeyBinding(L"m-b", "prevword"),	     KeyBinding(L"m-f", "nextword"),
    KeyBinding(L"return", "enter"),	     KeyBinding(L"up", "prevhistory"),
    KeyBinding(L"down", "nexthistory"),    KeyBinding(L"left", "prevchar"),
    KeyBinding(L"right", "nextchar"),	     KeyBinding(L"m-<", "scrollstart"),
    KeyBinding(L"m->", "scrollend"),	     KeyBinding(L"home", "firstchar"),
    KeyBinding(L"end", "lastchar"),	     KeyBinding(L"delete", "delete"),
    KeyBinding(L"pagedown", "scrolldown"), KeyBinding(L"pageup", "scrollup"),
    KeyBinding(L"fn.12", "toggleoverlay")
};

std::map<String32, KeyBindingMethod> keys;
std::map<String32, std::string> keystr;

void Runtime::addbinding(const wchar_t *key, const std::string &cmd)
{
	KeyBinding binding(key, cmd);
	KeyBindingMethod handler = binding.command();

	if (!handler)
	{
		grid->infof(_("/// missing handler for {} ({})\n"), mks(key), cmd);
	}
	else
	{
		keys[key] = handler;
		keystr[key] = cmd;
	}
}

void Runtime::initbindings()
{
	for (const auto &binding: initkeys)
	{
		KeyBindingMethod handler = binding.command();
		if (!handler)
		{
			grid->infof(_("/// missing handler for {} ({})\n"), mks(binding.s), binding.cmdname);
		}
		else
		{
			keys[binding.s] = handler;
			keystr[binding.s] = binding.cmdname;
		}
	}
}

void Runtime::dispatch_key(const String32 &s)
{
	if (s.length() == 1)
	{
		doinsertchar(s[0]);
		return;
	}

	if (keys.find(s) != keys.end())
	{
		KeyBindingMethod handler = keys[s];
		if (handler)
			(this->*handler)();
		else
			grid->infof(_("/// missing handler for {}\n"), mks(s));
		return;
	}

	if (s.length() > 3 && s[0] == 'f' && s[1] == 'n' && s[2] == '.')
	{
		std::string cs = mks(s);
		triggerfn(cs.c_str() + 3);
		return;
	}
}

void cmd_bind(Runtime *conn, const CommandArguments &arg)
{
	if (arg.size() != 1 && arg.size() != 3)
	{
		conn->grid->info(_("/// set [option value]\n"));
		return;
	}

	if (arg.size() == 1)
	{
		int wid = 0;
		for (std::map<String32, std::string>::const_iterator it = keystr.begin(); it != keystr.end(); it++)
			wid = std::max(wid, int(it->first.length()));

		for (std::map<String32, std::string>::const_iterator it = keystr.begin(); it != keystr.end(); it++)
			conn->grid->infof("{:>{}} {}\n", wid, mks(it->first), it->second);
	}
	else
	{
		std::string cmd;
		for (int i = 0; i < arg[2].size(); i++)
			cmd += arg[2][i];
		conn->addbinding(arg[1].c_str(), cmd.c_str());
	}
}
