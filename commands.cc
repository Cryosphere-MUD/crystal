#include "commands.h"

#include "crystal.h"
#include "grid.h"
#include "telnet.h"
#include "url.h"

void cmd_quit(conn_t *conn, const cmd_args &arg) {
  conn->quit = 1;
}

void cmd_z(conn_t *conn, const cmd_args &arg)
{
  conn->dosuspend();
}

void cmd_close(conn_t *conn, const cmd_args &arg)
{
  if (!conn->telnet) {
    conn->grid->info(_("/// you aren't connected.\n"));
    return;
  }

  delete conn->telnet;
  conn->telnet = 0;
  conn->grid->info(_("/// connection closed.\n"));
}

void cmd_reload(conn_t *conn, const cmd_args &arg)
{
  scripting::start();
  conn->grid->info(_("/// scripting restarted.\n"));
}

void cmd_help(conn_t *conn, const cmd_args &arg);

#ifdef MCCP
void cmd_compress(conn_t *conn, const cmd_args &arg) {
  if (conn->telnet &&
      conn->telnet->mc && 
      mudcompress_compressing(conn->telnet->mc)) {
    unsigned long compread, uncompread;
    mudcompress_stats(conn->telnet->mc, &compread, &uncompread);
    conn->grid->infof(_("/// wire:%lc bytes data:%lli bytes.\n"), compread, uncompread);
  } else {
    conn->grid->info(_("/// no compression\n"));
  }
}
#endif

void cmd_connect(conn_t *conn, const cmd_args &arg)
{
  if (arg.size()!= 2 && arg.size()!=3) {
    conn->grid->infof(_("/// connect <host> [port]\n"));
    return;
  }

  my_wstring host = arg[1];
  my_wstring port = arg.size()==3 ? arg[2] : L"";

  std::string cport = mks(port);
  std::string chost = mks(host);

  url u = url(chost.c_str());
  if (cport.length()!=0) {
    u.service = cport;
  }
  
  if (!valid_protocol(u.protocol)) {
    conn->grid->infof(_("/// bad protocol : '%s'.\n"), cport.c_str());
    return;
  }

  int p = lookup_service(u.service);
  if (p == -1) {
    conn->grid->infof(_("/// bad port : '%s'.\n"), cport.c_str());
    return;
  }

  conn->connect(u.hostname.c_str(), p, u.protocol=="telnets");
}

void cmd_match(conn_t *conn, const cmd_args &arg) {
  if (arg.size() != 2 && arg.size() != 1) {
    conn->grid->infof(_("/// match [pattern]\n"));    
    return;
  }

  if (arg.size()==1) {
    conn->hl_matches = std::set<my_wstring>();
  } else {
    conn->hl_matches.insert(arg[1]);
  }

  conn->grid->changed = 1;
}

void cmd_charset(conn_t *conn, const cmd_args &arg) 
{
  if (arg.size() != 2) {
    conn->grid->infof(_("/// charset <charset>\n"));
    return;
  }

  conn->mud_cset = mks(arg[1]);
  conn->grid->infof(_("/// charset '%s' selected\n"), conn->mud_cset.c_str());
}  

void cmd_dump(conn_t *conn, const cmd_args &arg)
{
  if (arg.size() != 2) {
    conn->grid->infof(_("/// dump <filename>\n"));
    return;
  }

  std::string cfilename = mks(arg[1]);
  conn->grid->file_dump(cfilename.c_str());
}

void cmd_log(conn_t *conn, const cmd_args &arg)
{
  if (arg.size() != 2) {
    conn->grid->infof(_("/// log <filename>\n"));
    return;
  }

  std::string cfilename = mks(arg[1]);
  conn->file_log(cfilename.c_str());
}

void cmd_dumplog(conn_t *conn, const cmd_args &arg)
{
  if (arg.size() != 2) {
    conn->grid->infof(_("/// dumplog <filename>\n"));
    return;
  }

  std::string cfilename = mks(arg[1]);
  if (conn->grid->file_dump(cfilename.c_str()))
    conn->file_log(cfilename.c_str());
}

struct option_t {
  const char *name;
  bool (conn_t::*option);
} options[] = {
  {"neverecho", &conn_t::never_echo },
  {"lpprompts", &conn_t::lp_prompts },
  { NULL, }
};

void cmd_bind(conn_t *conn, const cmd_args &arg);

void cmd_set(conn_t *conn, const cmd_args &arg)
{
  if (arg.size() != 1 && arg.size() != 3) {
    conn->grid->infof(_("/// set [option value]\n"));
    return;
  }

  bool to = false;

  if (arg.size()==1)
    conn->grid->infof(_("/// current options are\n"));
  else {
    if (arg[2]==L"on" || arg[2]==L"yes" || arg[2]==L"true" || arg[2]==L"1")
      to = true;
    else if (arg[2]==L"off" || arg[2]==L"no" || arg[2]==L"false" || arg[2]==L"0")
      to = false;
    else {
      conn->grid->infof(_("/// valid values are 'on' or 'off'\n"));
      return;
    }
  }

  std::string s = arg.size()==3?mks(arg[1]):"";

  for (int i=0;options[i].name;i++) {
    if (arg.size()==1)
      conn->grid->infof("///  %s - %s\n", options[i].name, conn->*options[i].option?"on":"off");
    else
      if (s==options[i].name) {
	conn->*options[i].option = to;
	conn->grid->infof("/// done\n");
	return;
      }
  }

  if (arg.size()==3)
    conn->grid->infof("/// no option of %ls\n", s.c_str());
}

struct cmd_t {
  const wchar_t *commandname;
  void (*function)(conn_t *conn, const std::vector<my_wstring> &);
  const char *args;
  const char *help;
} cmd_table[] =
{
  { L"connect" , cmd_connect, "<host> [port]", "connects to given host" },
  { L"open",     cmd_connect },
  { L"close",    cmd_close, NULL, "cuts connection" },

  { L"quit",     cmd_quit, NULL, "quits crystal" },
  { L"exit",     cmd_quit, NULL, NULL },

#ifdef MCCP
  { L"compress", cmd_compress, NULL, "show compression stats" },
#endif

  { L"dump",     cmd_dump, "<filename>", "dump scrollback to file" },
  { L"log", 	 cmd_log,  "<filename>", "log to file" },
  { L"dumplog",  cmd_dumplog, "<filename>", "dump scrollback to file and start logging to it" },

  { L"match", 	 cmd_match, "[pattern]", "highlight text matching pattern" },
  { L"charset",  cmd_charset, "<charset>", "talk to mud with given charset" },
#ifdef HAVE_LUA
  { L"reload",   cmd_reload, NULL, "reload config file" },
#endif
  { L"help",     cmd_help, NULL, "brief summary of commands" },

  { L"set",      cmd_set, "[option value]", "shows current options or sets one" },
  { L"bind",     cmd_bind, "[key value]", "shows or sets current keyboard bindings" },

  { L"z",        cmd_z, NULL, "suspend" },
  { NULL, }
};

void cmd_help(conn_t *conn, const cmd_args &arg) 
{
  int i = 0;
  while (cmd_table[i].commandname) {
    if (!i || cmd_table[i].function != cmd_table[i-1].function) {
      if (cmd_table[i].args)
	if (cmd_table[i].help)
	  conn->grid->infof("// %ls %s - %s\n", cmd_table[i].commandname, cmd_table[i].args, cmd_table[i].help);
	else
	  conn->grid->infof("// %ls %s\n", cmd_table[i].commandname, cmd_table[i].args);
      else
	if (cmd_table[i].help)
	  conn->grid->infof("// %ls - %s\n", cmd_table[i].commandname, cmd_table[i].help);
	else
	  conn->grid->infof("// %ls\n", cmd_table[i].commandname);
    }
    i++;
  }
}

std::vector<my_wstring> tokenize(my_wstring s) {
  std::vector<my_wstring> v;
  while (1) {
    my_wstring::size_type n = s.find(L' ');
    if (n == my_wstring::npos)
      break;
    v.push_back(s.substr(0, n));
    s = s.substr(n+1);
  }

  if (s.length()) {
    v.push_back(s);
  }
  
  return v;
}

void docommand(conn_t *conn, my_wstring s)
{
  std::vector<my_wstring> args = tokenize(s);
  
  int i = 0;
  while (const wchar_t *cm=cmd_table[i].commandname) {
    if (cm == args[0]) {
      cmd_table[i].function(conn, args);
      return;
    }
    i++;
  }
  
  conn->grid->info(_("/// don't understand that\n"));
}
