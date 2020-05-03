/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2020 white-rabbit, ISC license
*/

#pragma once

#include <string>
    
namespace DFPlex
{

// returns true if error.
bool log_begin(const std::string& path);

void log_end();
void log_message(const std::string& message);

}
