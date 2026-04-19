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

#include "commands.h"

#include <fmt/format.h>
#include <optional>

#include "Runtime.h"
#include "grid.h"
#include "telnet.h"
#include "url.h"

void cmd_quit(Runtime *conn, const CommandArguments &arg)
{
	conn->quit = true;
}

void cmd_z(Runtime *conn, const CommandArguments &arg)
{
	conn->dosuspend();
}

void cmd_close(Runtime *conn, const CommandArguments &arg)
{
	if (!conn->telnet)
	{
		conn->grid->info(_("/// you aren't connected.\n"));
		return;
	}

	conn->telnet.reset();
	conn->grid->info(_("/// connection closed.\n"));
}

void cmd_reload(Runtime *conn, const CommandArguments &arg)
{
	scripting::start();
	conn->grid->info(_("/// scripting restarted.\n"));
}

void cmd_help(Runtime *conn, const CommandArguments &arg);

void cmd_compress(Runtime *conn, const CommandArguments &arg)
{
	if (!conn->telnet)
	{
		conn->grid->info(_("/// you aren't connected.\n"));
		return;
	}

	switch (conn->telnet->compression_mode)
	{
	case 2:
		conn->grid->info(_("/// zlib compression active\n"));
		break;
	case 4:
		conn->grid->info(_("/// zstd compression active\n"));
		break;
	default:
		conn->grid->info(_("/// no compression\n"));
		break;
	}
}

void cmd_connect(Runtime *conn, const CommandArguments &arg)
{
	if (arg.size() < 2 || arg.size() > 5)
	{
		conn->grid->info(_("/// connect [-s] [-i keyfile] <host> [port]\n"));
		return;
	}

	bool force_tls = false;
	std::string key_path;
	int cmd_root = 1;

	while (cmd_root < (int)arg.size()) {
		if (arg[cmd_root] == L"-s") {
			force_tls = true;
			cmd_root++;
			continue;
		}
#ifdef HAVE_LIBSSH2
		if (arg[cmd_root] == L"-i" && cmd_root + 1 < (int)arg.size()) {
			key_path = mks(arg[cmd_root + 1]);
			cmd_root += 2;
			continue;
		}
#endif
		break;
	}

	if (cmd_root >= (int)arg.size())
	{
		conn->grid->info(_("/// connect [-s] [-i keyfile] <host> [port]\n"));
		return;
	}

	String32 host = arg[cmd_root];
	String32 port = (int)arg.size() == cmd_root + 2 ? arg[cmd_root + 1] : L"";

	std::string cport = mks(port);
	std::string chost = mks(host);

	URL u{chost};
	if (cport.length() != 0)
		u.service = cport;

	Runtime::ConnectionType type = Runtime::ConnectionType::Telnet;
	if (u.protocol == "telnets" || force_tls)
		type = Runtime::ConnectionType::TelnetSSL;
#ifdef HAVE_LIBSSH2
	if (u.protocol == "ssh")
		type = Runtime::ConnectionType::SSH;
#endif

	conn->connect(u.hostname, u.service, type,
	              u.has_username ? u.username : "",
	              u.has_password ? u.password : "",
	              key_path.empty() ? conn->ssh_default_key_path : key_path);
}

String32 join_from(const CommandArguments &args, int from)
{
	String32 ws;
	for (int i = from; i < args.size(); i++)
		ws += L" " + args[i];
	return ws.substr(1);
}

void cmd_match(Runtime *conn, const CommandArguments &arg)
{
	if (arg.size() == 1)
		conn->hl_matches = std::set<String32>();
	else
		conn->hl_matches.insert(join_from(arg, 1));

	conn->grid->changed = 1;
}

void cmd_charset(Runtime *conn, const CommandArguments &arg)
{
	if (arg.size() != 2)
	{
		conn->grid->info(_("/// charset <charset>\n"));
		return;
	}

	conn->mud_cset = mks(arg[1]);
	conn->grid->infof(_("/// charset '%s' selected\n"), conn->mud_cset);
}

void cmd_dump(Runtime *conn, const CommandArguments &arg)
{
	if (arg.size() != 2)
	{
		conn->grid->info(_("/// dump <filename>\n"));
		return;
	}

	std::string cfilename = mks(arg[1]);
	conn->grid->file_dump(cfilename);
}

void cmd_log(Runtime *conn, const CommandArguments &arg)
{
	if (arg.size() != 2)
	{
		conn->grid->info(_("/// log <filename>\n"));
		return;
	}

	std::string cfilename = mks(arg[1]);
	conn->file_log(cfilename);
}

void cmd_dumplog(Runtime *conn, const CommandArguments &arg)
{
	if (arg.size() != 2)
	{
		conn->grid->info(_("/// dumplog <filename>\n"));
		return;
	}

	std::string cfilename = mks(arg[1]);
	if (conn->grid->file_dump(cfilename))
		conn->file_log(cfilename);
}

struct option_t
{
	const char *name;
	bool(Runtime::*option);
};

auto options = {
    option_t{"neverecho", &Runtime::never_echo},
    option_t{"lpprompts", &Runtime::lp_prompts},
};

void cmd_bind(Runtime *conn, const CommandArguments &arg);

void cmd_set(Runtime *conn, const CommandArguments &arg)
{
	if (arg.size() != 1 && arg.size() != 3)
	{
		conn->grid->info(_("/// set [option value]\n"));
		return;
	}

	bool to = false;

	if (arg.size() == 1)
		conn->grid->info(_("/// current options are\n"));
	else
	{
		if (arg[2] == L"on" || arg[2] == L"yes" || arg[2] == L"true" || arg[2] == L"1")
			to = true;
		else if (arg[2] == L"off" || arg[2] == L"no" || arg[2] == L"false" || arg[2] == L"0")
			to = false;
		else
		{
			conn->grid->info(_("/// valid values are 'on' or 'off'\n"));
			return;
		}
	}

	std::string s = arg.size() == 3 ? mks(arg[1]) : "";

	for (const auto &opt : options)
	{
		if (arg.size() == 1)
			conn->grid->infof("///  {} - {}\n", opt.name, conn->*opt.option ? "on" : "off");
		else if (s == opt.name)
		{
			conn->*opt.option = to;
			conn->grid->info("/// done\n");
			return;
		}
	}

	if (arg.size() == 3)
		conn->grid->infof("/// no option of {}\n", s);
}

struct cmd_t
{
	String32 commandname;
	CommandHandler function;
	std::optional<std::string> args;
	std::optional<std::string> help;
};

std::vector<cmd_t> cmd_table = {
    cmd_t{L"connect", cmd_connect, "<host> [port]", "connects to given host"},
    {L"open", cmd_connect},
    {L"close", cmd_close, std::nullopt, "cuts connection"},

    {L"quit", cmd_quit, std::nullopt, "quits crystal"},
    {L"exit", cmd_quit, std::nullopt, std::nullopt},

    {L"compress", cmd_compress, std::nullopt, "show compression status"},

    {L"dump", cmd_dump, "<filename>", "dump scrollback to file"},
    {L"log", cmd_log, "<filename>", "log to file"},
    {L"dumplog", cmd_dumplog, "<filename>", "dump scrollback to file and start logging to it"},

    {L"match", cmd_match, "[pattern]", "highlight text matching pattern"},
    {L"charset", cmd_charset, "<charset>", "talk to mud with given charset"},
#ifdef HAVE_LUA
    {L"reload", cmd_reload, std::nullopt, "reload config file"},
#endif
    {L"help", cmd_help, std::nullopt, "brief summary of commands"},

    {L"set", cmd_set, "[option value]", "shows current options or sets one"},
    {L"bind", cmd_bind, "[key value]", "shows or sets current keyboard bindings"},

    {L"z", cmd_z, std::nullopt, "suspend"},
};

void cmd_help(Runtime *conn, const CommandArguments &arg)
{
	CommandHandler prev_func = nullptr;
	for (const auto &cmd : cmd_table)
	{
		if (cmd.function != prev_func)
		{
			if (cmd.args)
				if (cmd.help)
					conn->grid->infof("// {} {} - {}\n", mks(cmd.commandname), cmd.args.value(), cmd.help.value());
				else
					conn->grid->infof("// {} {}\n", mks(cmd.commandname), cmd.args.value());
			else if (cmd.help)
				conn->grid->infof("// {} - {}\n", mks(cmd.commandname), cmd.help.value());
			else
				conn->grid->infof("// {}\n", mks(cmd.commandname));
			prev_func = cmd.function;
		}
	}
}

std::vector<String32> tokenize(String32 s)
{
	std::vector<String32> v;
	while (1)
	{
		String32::size_type n = s.find(L' ');
		if (n == String32::npos)
			break;
		v.push_back(s.substr(0, n));
		s = s.substr(n + 1);
	}

	if (s.length())
		v.push_back(s);

	return v;
}

std::optional<std::string> nullopt_if_empty(const std::string &arg)
{
	if (arg.empty())
		return std::nullopt;
	return arg;
}

void register_command(const std::string &cmd, CommandHandler function, const std::string &arg, const std::string &hlp)
{
	cmd_t newCmd = {mkws(cmd), function, nullopt_if_empty(arg), nullopt_if_empty(hlp)};
	cmd_table.push_back(newCmd);
}

void docommand(Runtime *conn, String32 s)
{
	std::vector<String32> args = tokenize(s);

	for (const auto &cmd : cmd_table)
	{
		if (cmd.commandname == args[0])
		{
			cmd.function(conn, args);
			return;
		}
	}

	conn->grid->info(_("/// don't understand that\n"));
}
