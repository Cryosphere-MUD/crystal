/*
 * Crystal Mud Client
 * Copyright (C) Phil Christensen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef SSH_H
#define SSH_H

#ifdef HAVE_LIBSSH2

#include <memory>

class Runtime;

void ssh_begin_handshake(std::shared_ptr<Runtime> self);
void ssh_continue_auth(std::shared_ptr<Runtime> self);
void ssh_do_read(std::shared_ptr<Runtime> self);
void ssh_write(std::shared_ptr<Runtime> self, const std::string &data);
void ssh_disconnect(Runtime *self);
void ssh_check_never_echo_timeout(Runtime *conn);

#else

// Stub so callers compile cleanly without libssh2
class Runtime;
inline void ssh_disconnect(Runtime *) {}
inline void ssh_check_never_echo_timeout(Runtime *) {}

#endif

#endif
