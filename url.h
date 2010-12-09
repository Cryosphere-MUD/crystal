#ifndef URL_H
#define URL_H

#include <string>

inline bool valid_protocol(const std::string &s)
{
  if (s == "telnet")
    return 1;

#ifdef HAVE_SSL
  if (s == "telnets")
    return 1;
#endif
  
  return 0;
}

#endif
