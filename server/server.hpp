/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
*/

#pragma once

#include <ctime>
#include <map>
#include <string>
#include <websocketpp/server.hpp>

#include "Client.hpp"

namespace ws = websocketpp;
// FIXME: use unique_ptr or the boost equivalent
typedef ws::connection_hdl conn_hdl;

static std::owner_less<conn_hdl> conn_lt;
inline bool operator==(const conn_hdl& p, const conn_hdl& q)
{
    return (!conn_lt(p, q) && !conn_lt(q, p));
}
inline bool operator!=(const conn_hdl& p, const conn_hdl& q)
{
    return conn_lt(p, q) || conn_lt(q, p);
}

typedef std::map<conn_hdl, Client*, std::owner_less<conn_hdl>> conn_map;
extern conn_map clients;

// void* argument is actually DFHack::color_ostream*
// signature is constrained by tthread.
void wsthreadmain(void*);

size_t get_client_count();
Client* get_client(size_t n); // retrieves nth client.
Client* get_client(conn_hdl hdl); // retrieves client from connection
