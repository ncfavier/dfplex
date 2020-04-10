#pragma once

/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
 */

#include "Client.hpp"
#include "ColorText.h"
#include <fast_mutex.h>

extern tthread::fast_mutex dfplex_mutex;
extern bool global_pause;
extern bool plexing;
extern int32_t frames_elapsed;
extern DFHack::color_ostream* _out;
bool is_paused();
