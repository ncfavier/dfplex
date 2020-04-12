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

#include <mongoose/Server.h>
#include <mongoose/WebController.h>

using namespace Mongoose; 

void init_static(void*)
{
    WebController c;
    Server server(STATICPORT, STATICDIR.c_str());
    server.registerController(&c);

    server.start();
    *_out << "[DFPLEX] Static site server started on port " << STATICPORT << endl;
    *_out << "[DFPLEX] Connect to http://localhost:" << STATICPORT << "/dfplex.html in your browser." << endl;
    
    while (1) {
        DFHack::Core::getInstance().getConsole().msleep(60);
    }

}
