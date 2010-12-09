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

/*
 *
 *   stdin -> charset filter -> esc seq interpreter -> keycode interpreter -> 
 *                            cmd interpreter
 *
 *   read or sslread -> telnet decoder -> charset filter -> tty handler
 */

#include "crystal.h"
#include "io.h"
#include "grid.h"
#include "commandeditor.h"

#include <map>

hlist cmdhist;

hlist *conn_t::chist() {
  static hlist hist;
  
  if (commandmode)
    return &cmdhist;

  return &hist;
}

void conn_t::dokillword() {
  int orig=cursor;
  if (cursor) {
    while(cursor && isspace(buffer[cursor-1]))
      cursor--;
    while(cursor && !isspace(buffer[cursor-1]))
      cursor--;

    my_wstring extra = buffer.substr(orig);
    buffer = buffer.substr(0, cursor);
    buffer += extra;
  }
}

extern mterm tty;

void conn_t::dodelete() {
  if (cursor < buffer.length()) {
    buffer.erase(cursor, 1);
  } else {
    tty.beep();
  }
}

void conn_t::dobackspace()
{
  if (cursor) {
    buffer.erase(cursor-1, 1);
    cursor--;
  } else {
    tty.beep();
  }
}

void conn_t::doprevhistory() {
  if (chist() && chist()->back()) {
    if (nofuture) {
      future = buffer;
      nofuture = false;
    }

    buffer = chist()->get();
    cursor = buffer.length();
  }
  else
    tty.beep();
}

void conn_t::donexthistory()
{
  if (chist()) {
    if (chist()->next()) {
      buffer = chist()->get();
    }
    else {
      buffer = nofuture ? L"" : future;
      future = L"";
      nofuture = true;
    }
    cursor = buffer.length();
  }
  else
    tty.beep();
}

void conn_t::doprevchar()
{
  if (cursor)
    cursor--;
  else
    tty.beep();
}

void conn_t::doprevword()
{
  while (cursor && !isalnum(buffer[cursor-1])) {
    cursor--;
  }
  while (cursor && isalnum(buffer[cursor-1])) {
    cursor--;
  }
}

void conn_t::donextword()
{
  while (cursor<buffer.length() && !isalnum(buffer[cursor])) {
    cursor++;
  }
  while (cursor<buffer.length() && isalnum(buffer[cursor])) {
    cursor++;
  }
}

void conn_t::donextchar()
{
  cursor++;
  if (cursor>buffer.length()) {
    cursor = buffer.length();
    tty.beep();
  }
}

void conn_t::dofirstchar() {
  cursor = 0;
}

void conn_t::doclearline() {
  buffer = L"";
  cursor = 0;
}

void conn_t::dolastchar() {
  cursor = buffer.length();
}

void conn_t::dotranspose() {
  if (cursor>1 && buffer.length()==cursor) {
    wchar_t tmp = buffer[cursor-1];
    buffer[cursor-1] = buffer[cursor-2];
    buffer[cursor-2] = tmp;
    grid->changed = 1;
  } else if (cursor>0) {
    wchar_t tmp = buffer[cursor];
    buffer[cursor] = buffer[cursor-1];
    buffer[cursor-1] = tmp;
    cursor++;
    grid->changed = 1;
  } else {
    tty.beep();
  }
}

void conn_t::doinsertchar(wchar_t ch)
{
  if (cursor == buffer.length()) {
    buffer += ch;
  } else {
    buffer = buffer.substr(0, cursor) + ch + buffer.substr(cursor);
  }
  grid->changed = 1;
  cursor++;
}

void conn_t::docutfromhere() {
  cutbuffer=buffer.substr(cursor);
  buffer = buffer.substr(0, cursor);
}

void conn_t::docuttohere() {
  cutbuffer = buffer.substr(0, cursor);
  buffer = buffer.substr(cursor);
  cursor = 0;
}

void conn_t::dopaste() 
{
  buffer.insert(cursor, cutbuffer);
}
