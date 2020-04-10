/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
*/

#pragma once

#include <cstdint>
#include <vector>
#include <string>

extern bool AUTOSAVE_WHILE_IDLE;
extern uint32_t MAX_CLIENTS;
extern uint16_t PORT;
extern uint16_t STATICPORT;
extern std::string STATICDIR;
extern uint32_t UNIPLEXKEY;
extern uint32_t DEBUGKEY;
extern uint32_t SERVERDEBUGKEY;
extern uint32_t NEXT_CLIENT_POS_KEY;
extern uint32_t PREV_CLIENT_POS_KEY;
extern bool CURSOR_IS_TEXT;
extern bool NOPAUSE;
extern std::string SECRET;

bool load_config();
std::vector<std::string> split(const char *str, char c = ' ');