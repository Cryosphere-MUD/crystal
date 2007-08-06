#ifndef SCRIPTING_H
#define SCRIPTING_H

class grid_t;

namespace scripting {
  void start();

  void set_grid(grid_t *);

  int count_timers();
  
  void dotimers();
  void dotrigger(const my_wstring &s);
  void doprompt(const my_wstring &s);

  std::string lookup_host(const std::string &h);
};

#endif
