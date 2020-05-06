/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2020 white-rabbit, ISC license
*/

#pragma once

#include "Client.hpp"
#include <functional>

namespace DFPlex
{
    // triggered after a state restore.
    void add_cb_post_state_restore(std::function<void(Client*)>&&);
    void run_cb_post_state_restore(Client*);
    
    // triggered during shutdown of the plugin.
    void add_cb_shutdown(std::function<void()>&&);
    void run_cb_shutdown();
    
    // removes all callbacks.
    void cleanup_callbacks();
}