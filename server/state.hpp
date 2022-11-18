#pragma once

/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
 */

#include <string>

struct Client;

enum class RestoreResult
{
    SUCCESS,
    FAIL,
    ABORT_PLEX, // failed so bad we should switch to uniplex mode
};
extern std::string restore_state_error;
RestoreResult restore_state(Client* client);

void deferred_state_restore(Client*);

void capture_post_state(Client* client);
