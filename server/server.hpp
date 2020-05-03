/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
*/

#pragma once


#include <ctime>
#include <map>
#include <string>

#include "Client.hpp"

extern std::set<Client*> clients;

// used to launch server by dfplex.
void wsthreadmain(void*);

