/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2020 white-rabbit, ISC license
*/

#include "parse_config.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

std::vector<config_symbol> parse_config_file(const std::string& path_to_raw)
{
    std::ifstream f(path_to_raw);
    std::vector<config_symbol> symbols;
	if (!f.is_open()) {
		std::cerr << "Dwarfplex failed to open config file, skipping." << std::endl;
		return symbols;
	}

    std::string line;
    while(getline(f, line))
    {
        size_t offset = 0;
        while (true)
        {
            size_t seek = line.find("[", offset);
            if (seek != std::string::npos)
            {
                offset = line.find("]", offset);
                if (offset == std::string::npos)
                {
                    break;
                }

                // to parse interface.txt
                if (offset == line.length() - 3 && line.at(line.length() - 2) == ']')
                {
                    offset++;
                }

                // found [ and ] tokens, now find :
                size_t sep_index = seek;
                bool set_op = false;
                symbols.emplace_back();
                config_symbol& symbol = symbols.back();
                while (sep_index < offset)
                {
                    size_t start_index = sep_index + 1;

                    // to parse interface.txt
                    size_t add_search = 0;
                    if (set_op && symbol.op == "KEY") add_search = 1;

                    sep_index = line.find(":", start_index + add_search);
                    if (sep_index == std::string::npos)
                    {
                        // last match is ':'
                        sep_index = offset;
                    }

                    std::string sub = line.substr(start_index, sep_index - start_index);

                    if (set_op)
                    {
                        symbol.args.emplace_back(std::move(sub));
                    }
                    else
                    {
                        symbol.op = std::move(sub);
                        set_op = true;
                    }
                }
            }
            else
            {
                break;
            }
        }
    }
    return symbols;
}

std::ostream& operator<<(std::ostream& out, const config_symbol& match)
{
    out << "[" << match.op;
    for (const std::string& arg : match.args)
    {
        out << ":" << arg;
    }
    out << "]";
    return out;
}
