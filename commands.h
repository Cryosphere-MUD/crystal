#ifndef COMMANDS_H
#define COMMANDS_H

#include <vector>

#include "common.h"

typedef std::vector<my_wstring> cmd_args;

class conn_t;

void docommand(conn_t *conn, my_wstring s);

#endif
