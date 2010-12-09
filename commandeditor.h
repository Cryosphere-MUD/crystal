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
