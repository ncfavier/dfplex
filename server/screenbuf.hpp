/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
 */
 
#pragma once 
 
#include "Client.hpp"

extern screenbuf_t screenbuf;

void hook_renderer();

void unhook_renderer();

void scrape_screenbuf(Client* cl);

void transfer_screenbuf_client(Client* client);

void transfer_screenbuf_to_all();

void perform_render(int32_t w, int32_t h);

void restore_render();