#include "common.h"

std::string mks(const my_wstring &w)
{
  std::string s;
  for (size_t i=0;i<w.length();i++) {
    wchar_t c = w[i];
    if (c < 0x80)
      s += c;
    else {
      char t[100];
      sprintf(t, "%lc", c);
      s += t;
    }
  }
  return s;
}
