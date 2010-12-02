#ifndef CRYSTAL_H
#define CRYSTAL_H

struct telnet_state;
class grid_t;
class InAddrList;

class conn_t {
public:
  //! are we in never echo mode (default: no)
  bool never_echo;

  //! are we in kludge lp prompts mode (default: yes)
  bool lp_prompts;

  //! are we quitting?
  bool quit;

  InAddrList *addrs;
  int addr_i;

  std::string host;
  int port;
  bool ssl;

  grid_t *grid;
  grid_t *slave;

  grid_t *cur_grid;

  telnet_state *telnet;

  my_wstring buffer;
  size_t cursor;

  my_wstring cutbuffer;

  FILE *logfile;

  std::string mud_cset;

  void doprevhistory();
  void donexthistory();

  void doclearline();
  void dorefresh();
  void dotranspose();
  void doinsertchar(wchar_t);
  void dobackspace();

  void dolastchar();
  void donextchar();
  void doprevchar();
  void dofirstchar();

  void doprevword();
  void donextword();
  
  void dokillword();
  void dodelete();

  void docutfromhere();
  void docuttohere();
  void dopaste();

  void doscrollup();
  void doscrolldown();
  void doscrollstart();
  void doscrollend();
  void triggerfn(const char *which);
  void findnext();

  void doenter();
  void dosuspend();
  void dotoggleslave();
  void docommandmode();

  void show_lines_at(int from, int to, int num);
  
  ~conn_t();
  conn_t(grid_t *);

  void dispatch_key(const my_wstring &s);
};

#endif
