/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2020 white-rabbit, ISC license
*/

#pragma once

#include <string>
#include <vector>
#include <ostream>

struct config_symbol
{
    std::string op;
    std::vector<std::string> args;
};

std::ostream& operator<<(std::ostream& a, const config_symbol& match);

std::vector<config_symbol> parse_config_file(const std::string& path_to_raw);
