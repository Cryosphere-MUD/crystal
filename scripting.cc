/*
 * Crystal Mud Client
 * Copyright (C) 2002-2010 Abigail Brady
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

#include "common.h"
#include "crystal.h"
#include "grid.h"
#include "scripting.h"
#include "Socket.h"
#include "telnet.h"
#include "commands.h"

#ifndef HAVE_LUA

namespace scripting {
  void start(void) { }
  void set_grid(grid_t *) { }
  void doprompt(const my_wstring &s) { }
  void dotrigger(const my_wstring &s) { }
  int count_timers() { return 0; }
  void dotimers() { };
  std::string lookup_host(const std::string &s) { return ""; }
};

#else

extern "C" {

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

}

lua_State *l = 0;
int badlua = 0;

namespace scripting {

  grid_t *ergrid = 0;

#ifdef HAVE_LUA50  
  void set_lua_error(lua_State* L, const char *errorString)
  {
    lua_pushstring(L, errorString);
    lua_error(L);
  }

  void do_lua_call(lua_State* L, int args, int rets)
  {
    if (lua_pcall(L, args, rets, 0) != 0) {
      std::string s = lua_tostring(L, -1);
      ergrid->infof("lua error: %s\n", s.c_str());
      badlua = 1;
    }
  }

  lua_State* do_lua_open()
  {
    return lua_open();
  }
#else
  void set_lua_error(lua_State* L, const char *errorString)
  {
    lua_error(L, errorString);
  }    

  void do_lua_call(lua_State* L, int args, int rets)
  {
    lua_call(L, args, rets);
  }

  lua_State* do_lua_open()
  {
    return lua_open(100);
  }
#endif

  void kill_lua() {
    l = 0;
  }

  void kill_if_bad() {
    static int i = 0;
    if (badlua) {
      i++;
      badlua = 0;
      if (l) {
	kill_lua();
      }
      badlua = 0;
      l = 0;
    }
  }

  struct timer {
    time_t next;
    int howoften;
    std::string callwhat;
    void call() {
      time_t now = time(NULL);
      if (now < next)
	return;
    lua_getglobal(l, callwhat.c_str());
    do_lua_call(l, 0, 0);
    next = now + howoften;
    }
  };
  
  std::list<timer> timers;
  
  int count_timers() {
    return timers.end() != timers.begin();
  }

  void set_grid(grid_t *g) {
    ergrid = g;
  }

  void dotimers()
  {
    if (!l)
      return;
    {
      std::list<timer>::iterator i = timers.begin();
      while (i != timers.end()) {
	i->call();
	i++;
      }
    } 
 }

void tomud_echo(std::string proper)
{
  conn_t *conn = ergrid->conn;

  if (conn->grid->cstoredprompt.length()) {
    for (int i=0;i<conn->grid->cstoredprompt.length();i++) {
      conn->grid->place(&conn->grid->cstoredprompt[i]);
    }
    conn->grid->cstoredprompt.erase();
  }

  conn->grid->lastprompt = 0;
    
  if (!(conn->telnet && conn->telnet->will_echo)) {
    my_wstring a;
    for (int i=0;i<conn->grid->col;i++) {
      a += conn->grid->get(conn->grid->row, i).ch;
    }    
    for (size_t i=0;i<proper.length();i++) {
      cell_t c = proper[i];
      conn->grid->place(&c);
    }
    conn->grid->wantnewline();
    conn->grid->changed = 1;
  }

  proper += "\r\n";
  if (ergrid && ergrid->conn->telnet) {
    ergrid->conn->telnet->send(proper);
  }
}

static int lua_tomud(lua_State *L) {
  if (lua_gettop(L)!=1) {
    set_lua_error(L, "Bad number of args to tomud");
  }
  if (!lua_isstring(L, 1)) {
    set_lua_error(L, "Wanted std::string for tomud.");
  }
  if (ergrid && ergrid->conn && ergrid->conn->telnet) {
    std::string s = lua_tostring(L, 1);
    s += "\r\n";
    ergrid->conn->telnet->send(s.c_str());
  }
  return 0;
}

static int lua_tomud_echo(lua_State *L) {
  if (lua_gettop(L)!=1) {
    set_lua_error(L, "Bad number of args to tomud");
  }
  if (!lua_isstring(L, 1)) {
    set_lua_error(L, "Wanted std::string for tomud.");
  }
  if (ergrid && ergrid->conn && ergrid->conn->telnet) {
    std::string s = lua_tostring(L, 1);
    tomud_echo(s);
  }
  return 0;
}

static int lua_info(lua_State *L) {
  if (lua_gettop(L)!=1) {
    set_lua_error(L, "Bad number of args to tomud");
  }
  if (!lua_isstring(L, 1)) {
    set_lua_error(L, "Wanted std::string for tomud.");
  }
  if (ergrid) {
    ergrid->infof("/// %s", lua_tostring(L, 1));
  }
  return 0;
}

static int lua_register_auto(lua_State *L)
{
  if (lua_gettop(L)!=2) {
    set_lua_error(L, "Bad number of args to register_auto");
  }
  if (!lua_isstring(L, 1)) {
    set_lua_error(L, "Bad arg 1 to register_auto");
  }
  if (!lua_isnumber(L, 2)) {
    set_lua_error(L, "Bad arg 2 to register_auto");
  }
  timer a;
  a.callwhat = lua_tostring(L, 1);
  a.howoften = int(lua_tonumber(L, 2));
  if (a.howoften < 1) a.howoften = 1;
  a.next = time(NULL) + a.howoften;
  timers.insert(timers.begin(), a);
  return 0;
}

std::string trig = "", prompt = "", host = "";

static int triggered=0;

void dotrigger(const my_wstring &s) {
  if (!l)
    return;
  if (triggered || s[0]=='/')
    return;
  triggered = 1;
  std::string rs;
  for (size_t i=0;i<s.size();i++) {
    rs += s[i];
  }
  if (trig.length()) {
    lua_getglobal(l, trig.c_str());
    lua_pushstring(l, rs.c_str());
    do_lua_call(l, 1, 0);
  }
  triggered = 0;
  kill_if_bad();
}

void doprompt(const my_wstring &s) {
  if (!l)
    return;
  if (triggered || s[0]=='/')
    return;
  triggered = 1;
  std::string rs;
  for (size_t i=0;i<s.size();i++) {
    rs += s[i];
  }
  if (prompt.length()) {
    lua_getglobal(l, prompt.c_str());
    lua_pushstring(l, rs.c_str());
    do_lua_call(l, 1, 0);
  }
  triggered = 0;
  kill_if_bad();
}

static int lua_register_trig(lua_State *L)
{
  if (lua_gettop(L)!=1) {
    set_lua_error(L, "Bad number of args to register_trig");
  }
  if (!lua_isstring(L, 1)) {
    set_lua_error(L, "Bad arg 1 to register_trig");
  }
  trig = lua_tostring(L, 1);
  return 0;
}

static int lua_bind_key(lua_State *L)
{
  if (lua_gettop(L)!=2) {
    set_lua_error(L, "Bad number of args to bind_key");
  }
  if (!lua_isstring(L, 1)) {
    set_lua_error(L, "Bad arg 1 to bind_key");
  }
  if (!lua_isstring(L, 2)) {
    set_lua_error(L, "Bad arg 2 to bind_key");
  }
  const char* key = lua_tostring(L, 1);
  const char* cmd = lua_tostring(L, 2);
  conn_t *conn = ergrid->conn;

  my_wstring wkey;
  while (*key) {
    wkey += *key;
    key++;
  }

  conn->addbinding(wkey.c_str(), cmd);
  return 0;
}

static int lua_register_host(lua_State *L)
{
  if (lua_gettop(L)!=1) {
    set_lua_error(L, "Bad number of args to register_host");
  }
  if (!lua_isstring(L, 1)) {
    set_lua_error(L, "Bad arg 1 to register_host");
  }
  host = lua_tostring(L, 1);
  return 0;
}

static std::map<std::wstring, std::string> lua_command_functions;

static void lua_command_handler(conn_t *conn, const cmd_args &args)
{
  if (!l) {
    return;
  }

  lua_getglobal(l, lua_command_functions[args[0]].c_str());
  lua_newtable(l);

  for (int i = 0; i < args.size(); i++) {
    lua_pushnumber(l, i + 1);
    lua_pushstring(l, mks(args[i]).c_str());
    lua_settable(l, -3);
  }
  
  do_lua_call(l, 1, 0);
}

static int lua_register_command(lua_State *L)
{
  if (lua_gettop(L)!=4) {
    set_lua_error(L, "Bad number of args to register_command");
  }
  if (!lua_isstring(L, 1)) {
    set_lua_error(L, "Bad arg 1 to register_command");
  }
  const char* cmd = lua_tostring(L, 1);
  const char* lua = lua_tostring(L, 2);
  const char* arg = lua_tostring(L, 3);
  const char* hlp = lua_tostring(L, 4);
  lua_command_functions[mkws(cmd)] = lua;
  register_command(cmd, lua_command_handler, arg, hlp);
  return 0;
}

static int lua_quit(lua_State *L)
{
  if (lua_gettop(L)!=1) {
    set_lua_error(L, "Bad number of args to quit");
  }
  if (!lua_isnumber(L, 1)) {
    set_lua_error(L, "Bad arg 1 to register_command");
  }
  long num = lua_tonumber(L, 1);
  exitValue = num;
  conn_t *conn = ergrid->conn;
  conn->quit = true;
  return 0;
}

static int lua_register_prompt(lua_State *L)
{
  if (lua_gettop(L)!=1) {
    set_lua_error(L, "Bad number of args to register_auto");
  }
  if (!lua_isstring(L, 1)) {
    set_lua_error(L, "Bad arg 1 to register_auto");
  }
  prompt = lua_tostring(L, 1);
  return 0;
}

static int lua_get_host(lua_State *L)
{
  lua_pushstring(L, ergrid->conn->host.c_str());
  return 1;
}

static int lua_get_port(lua_State *L)
{
  lua_pushnumber(L, ergrid->conn->port);
  return 1;
}

static int luaerror(lua_State *L)
{
  badlua = 1;
  ergrid->infof("/// lua error: %s\n", lua_tostring(L,1));
  return 0;
}

std::string getRcFile() {
  if (const char* cry = getenv("CRYSTAL_LUA"))
    return cry;

  if (const char* home = getenv("HOME")) {
    return home + std::string("/.crystal.lua");
  }

  return "";
}

void start()
{
  if (l) {
    kill_lua();
    badlua = 0;
  }

  trig = "";
  host = "";
  prompt = "";
 
  l = do_lua_open();
  badlua = 0;

  lua_baselibopen(l);
  lua_iolibopen(l);
  lua_strlibopen(l);
  lua_mathlibopen(l);
#ifdef HAVE_LUA50  
  lua_tablibopen(l);
#endif

  lua_register(l, "_ERRORMESSAGE",luaerror);
  lua_register(l, "register_auto",lua_register_auto);
  lua_register(l, "register_trig",lua_register_trig);
  lua_register(l, "register_prompt",lua_register_prompt);
  lua_register(l, "register_host",lua_register_host);
  lua_register(l, "register_command",lua_register_command);
  lua_register(l, "bind_key",lua_bind_key);

  lua_register(l, "get_host",lua_get_host);
  lua_register(l, "get_port",lua_get_port);

  lua_register(l, "tomud",lua_tomud);
  lua_register(l, "tomud_echo",lua_tomud_echo);
  lua_register(l, "info",lua_info);

  lua_register(l, "quit", lua_quit);

  //  get_rcfile();

  std::string f = getRcFile();
  if (f.length()) {
    lua_dofile(l, f.c_str());
  }

  if (badlua) {
    lua_close(l);
    l = 0;
    badlua = 0;
  }

  //  kill_if_bad();
}

std::string lookup_host(const std::string &h)
{
  if (!l || badlua)
    return "";

  if (host.length()) {
    lua_getglobal(l, host.c_str());
    lua_pushstring(l, h.c_str());
    do_lua_call(l, 1, 1);
    const char *s = lua_tostring(l, 1);
    lua_pop(l, 1);
    if (s)
      return s;

    kill_if_bad();
  }

  return "";
}

}
#endif
