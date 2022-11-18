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

#ifndef _WIN32
    // this seems to cause build errors on windows.
    #define SO_REUSEPORT
#endif

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
        ss << "config.protocol = '" << WF_VERSION << "';\n";
        res.set_content(ss.str().c_str(), "application/javascript");
    });

    if (!ret)
    {
        Core::printerr("[DFPLEX] Failed to serve static site files from \"%s\"\n", STATICDIR.c_str());
    }
    else
    {
        Core::print("[DFPLEX] Static site server starting on port %d", STATICPORT);
        Core::print("[DFPLEX] Serving files from directory %s", STATICDIR.c_str());
        Core::print("[DFPLEX] Connect to http://localhost:%d/dfplex.html in your browser.", STATICPORT);
        svr.listen("0.0.0.0", STATICPORT);
    }
}
