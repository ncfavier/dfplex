/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
 */

#pragma once

// this is a hacky fix for a windows build error.
#include "modules/EventManager.h"

#include "Client.hpp"
#include "ColorText.h"
#include "chat.hpp"
#include <tinythread.h>

extern tthread::mutex dfplex_mutex;
extern bool global_pause;
extern bool plexing;
extern int32_t frames_elapsed;

extern ChatLog g_chatlog;

bool is_paused();

// please make sure that dfplex_mutex is locked before calling, regardless
// of callee thread.
size_t get_client_count();
Client* get_client_by_id(client_long_id_t id); // retrieves client by unique number assigned to each client.
Client* get_client(int32_t n); // retrieves nth client.
Client* get_client(const ClientIdentity*); // retrieves client by identity
int get_client_index(const ClientIdentity*); // retrieves client index by identity (or -1 if not found)

// creates a new client
Client* add_client();

// creates a new client and sets its callback function.
// (see Client.hpp documentation on Client::update_cb)
Client* add_client(client_update_cb&&);

void remove_client(Client*);