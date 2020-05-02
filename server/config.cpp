/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2020 white-rabbit, ISC license
*/

#include "config.hpp"
#include "Core.h"

bool AUTOSAVE_WHILE_IDLE = 0;
uint32_t MAX_CLIENTS = 0;
uint16_t PORT = 1234;
uint16_t STATICPORT = 8000;
std::string STATICDIR = "hack/www";
uint32_t MULTIPLEXKEY = 0;
uint32_t CHATKEY = 0;
uint32_t CHAT_NAME_KEY = 0;
bool CHAT_NAME_REQUIRED = 1;
uint32_t NEXT_CLIENT_POS_KEY = 0;
uint32_t PREV_CLIENT_POS_KEY = 0;
bool CURSOR_IS_TEXT = false;
PauseBehaviour PAUSE_BEHAVIOUR = PauseBehaviour::EXPLICIT;
uint32_t DEBUGKEY = 0;
uint32_t SERVERDEBUGKEY = 0;
uint16_t MESSAGE_TIME = 60 * 18;
uint16_t MESSAGE_FLASH_TIME = 10;
size_t MAX_MESSAGE_COUNT = 0x10000;
bool CHAT_ENABLED = true;
bool MULTISIZE = true;
std::string SECRET = ""; // auth is disabled by default

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
using namespace std; // iostream heavy

vector<string> split(const char *str, char c)
{
    vector<string> result;
    do {
        const char *begin = str;

        while(*str != c && *str)
            str++;

        result.push_back(string(begin, str));
    } while (0 != *str++);

    return result;
}

bool load_text_file()
{
	ifstream f("data/init/dfplex.txt");
	if (!f.is_open()) {
		DFHack::Core::printerr("Webfort failed to open config file, skipping.\n");
		return false;
	}

	string line;
	while(getline(f, line)) {
		size_t b = line.find("[");
		size_t e = line.rfind("]");

		if (b == string::npos || e == string::npos || line.find_first_not_of(" ") < b)
			continue;

		line = line.substr(b+1, e-1);
		vector<string> tokens = split(line.c_str(), ':');
		const string& key = tokens[0];
		const string& val = tokens[1];

		if (key == "PORT") {
			PORT = (uint16_t)std::stol(val);
		}
        if (key == "STATICPORT") {
			STATICPORT = (uint16_t)std::stol(val);
		}
		if (key == "MAX_CLIENTS") {
			MAX_CLIENTS = (uint32_t)std::stol(val);
		}
		if (key == "PAUSE_BEHAVIOUR" || key == "PAUSE")
        {
		    if (val == "ALWAYS")
            {
                PAUSE_BEHAVIOUR = PauseBehaviour::ALWAYS;
            }
            else if (val == "EXPLICIT")
            {
                PAUSE_BEHAVIOUR = PauseBehaviour::EXPLICIT;
            }
            else if (val == "DWARFMENU" || val == "EXPLICIT_DWARFMENU")
            {
                PAUSE_BEHAVIOUR = PauseBehaviour::EXPLICIT_DWARFMENU;
            }
            else if (val == "ANYMENU" || val == "EXPLICIT_ANYMENU")
            {
                PAUSE_BEHAVIOUR = PauseBehaviour::EXPLICIT_ANYMENU;
            }
            /*else if (val == "NEVER")
            {
                PAUSE_BEHAVIOUR = PauseBehaviour::NEVER;
            }*/
            else
            {
        		DFHack::Core::printerr("Pause behaviour not recognized.\n");
        		return false;
            }
        }
		if (key == "CURSOR_IS_TEXT")
        {
		    CURSOR_IS_TEXT = std::stol(val);
        }
	    if (key == "PREV_CLIENT_POS_KEY")
        {
		    PREV_CLIENT_POS_KEY = std::stol(val);
        }
	    if (key == "NEXT_CLIENT_POS_KEY")
        {
		    NEXT_CLIENT_POS_KEY = std::stol(val);
        }
        if (key == "DEBUGKEY" || key == "DEBUG_KEY")
        {
           DEBUGKEY = std::stol(val);
        }
        if (key == "SERVERDEBUGKEY" || key == "SERVER_DEBUG_KEY")
        {
           SERVERDEBUGKEY = std::stol(val);
        }
        if (key == "MULTIPLEXKEY" || key == "MULTIPLEX_KEY") {
            MULTIPLEXKEY = std::stol(val);
		}
        if (key == "CHATKEY" || key == "CHAT_KEY") {
            CHATKEY = std::stol(val);
            CHAT_ENABLED = !!CHATKEY;
		}
        if (key == "CHAT_NAME_KEY" || key == "CHATNAMEKEY") {
            CHAT_NAME_KEY = std::stol(val);
		}
        if (key == "CHAT_NAME_REQUIRED") {
            CHAT_NAME_REQUIRED = !!std::stol(val);
		}
	}
    
    // conditionals
    if (!CHAT_NAME_KEY) CHAT_NAME_REQUIRED = false;
    if (!CHAT_NAME_KEY || !CHATKEY) CHAT_ENABLED = false;
    
	return true;
}

bool load_env_vars()
{
    char* tmp;
	if ((tmp = getenv("DFPLEX_PORT"))) {
		PORT = (uint16_t)std::stol(tmp);
	}
    if ((tmp = getenv("DFPLEX_STATICPORT"))) {
		STATICPORT = (uint16_t)std::stol(tmp);
	}
	if ((tmp = getenv("DFPLEX_MAX_CLIENTS"))) {
		MAX_CLIENTS = (uint32_t)std::stol(tmp);
	}
	if ((tmp = getenv("DFPLEX_SECRET"))) {
		SECRET = tmp;
	}
    if ((tmp = getenv("DFPLEX_STATICDIR"))) {
		STATICDIR = tmp;
	}
	return true;
}

bool load_config()
{
	return load_text_file() && load_env_vars();
}
