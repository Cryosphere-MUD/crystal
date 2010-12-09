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

#ifndef COMMANDEDITOR_H
#define COMMANDEDITOR_H

#include <map>
#include "common.h"

struct hlist {
  std::map<int, my_wstring> hist;
  int idx;
  int max;
  hlist() : idx(0), max(0) {
  }

  void insert(const my_wstring &blah) {
    hist[max] = blah;
    max++;
    idx = max;
  }

  bool back() {
    idx--;
    if (idx < 0) {
      idx = 0;
      return 0;
    }
    return 1;
  }

  bool next() {
    idx++;
    if (idx >= max) {
      idx = max;
      return  0;
    }
    return 1;
  }

  const my_wstring &get() {
    static const my_wstring blah = L"";
    if (idx >= max || idx < 0) 
      return blah;
    return hist[idx];
  }
};

extern hlist cmdhist;

class commandeditor_t {
 public:
  //! are we in command mode (true) or mud mode (false)
  bool commandmode;
  
  my_wstring buffer;
  size_t cursor;

  my_wstring future;
  bool nofuture;

  my_wstring cutbuffer;

  hlist *chist();

  void doprevhistory();
  void donexthistory();

  void dokillword();
  void dodelete();

  void docutfromhere();
  void docuttohere();
  void dopaste();

  void dolastchar();
  void donextchar();
  void doprevchar();
  void dofirstchar();

  void doprevword();
  void donextword();

  void dotranspose();

  void doclearline();
  void doinsertchar(wchar_t);
  
  void dobackspace();

  
};

#endif
