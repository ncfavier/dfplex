/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2020 white-rabbit, ISC license
*/


#include "serverlog.hpp"
#include <fstream>

namespace DFPlex
{

static std::fstream logfile;
static bool is_open = false;

bool log_begin(const std::string& path)
{
    if (!is_open)
    {
        logfile.open(path, std::ios_base::app);

        if (!logfile.good()) return true;

        is_open = true;
    }

    return false;
}

void log_end()
{
    if (is_open)
    {
        logfile.close();
        is_open = false;
    }
}

void log_message(const std::string& message)
{
    if (is_open)
    {
        logfile << message << std::endl;
        logfile.flush();
    }
}

}
