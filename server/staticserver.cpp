/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
 */

#include "staticserver.hpp"
#include "config.hpp"
#include "dfplex.hpp"

#include "Console.h"
#include "Core.h"

#include <httplib.h>

using namespace DFHack;

void init_static(void*)
{
    using namespace httplib;

    Server svr;
    //Server server(STATICPORT, STATICDIR.c_str());
    
    auto ret = svr.set_mount_point("/", STATICDIR.c_str());
    svr.Get("/", [](const Request& req, Response& res) {
        res.set_redirect("dfplex.html");
    });
    
    svr.Get("/config-srv.js", [](const Request& req, Response& res) {
        std::stringstream ss;
        ss << "// This file is dynamically generated.\n";
        ss << "config.port = '" << PORT << "';\n";
        res.set_content(ss.str().c_str(), "application/javascript");
    });
    
    if (!ret)
    {
        _out->color(COLOR_RED);
        *_out << "[DFPLEX] Failed to serve static site files from \"" << STATICDIR.c_str()
        << "\"" << std::endl;
        _out->color(COLOR_RESET);
    }
    else
    {
        *_out << "[DFPLEX] Static site server starting on port " << STATICPORT << endl;
        *_out << "[DFPLEX] Serving files from directory " << STATICDIR << endl;
        *_out << "[DFPLEX] Connect to http://localhost:" << STATICPORT << "/dfplex.html in your browser." << endl;
        svr.listen("0.0.0.0", STATICPORT);
    }
}
