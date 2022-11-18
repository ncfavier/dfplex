/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2020 white-rabbit, ISC license
*/

#pragma once

#include "Client.hpp"
#include "df/interface_key.h"

// call once at start.
// error -> returns true
bool command_init();

// analyze and possibly apply the given command from the user.
// -- parameters --
//    keys: the command to be applied; as an optimization,
//          this is passed as a non-const reference and will be emptied.
//     cl: the client applying the command
//    raw: skip analysis (except for a permissions check)

void apply_command(std::set<df::interface_key>&, Client* cl, bool raw);

// permits a deferred rewind later.
void rewind_keyqueue_to_catch(Client* client);
