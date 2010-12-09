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

#endif
