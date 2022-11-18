/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2020 white-rabbit, ISC license
*/

#include "callbacks.hpp"
#include "dfplex.hpp"

namespace DFPlex
{

std::vector<std::function<void(Client*)>> g_cb_post_state_restore;
std::vector<std::function<void()>> g_cb_shutdown;

void add_cb_post_state_restore(std::function<void(Client*)>&& fn)
{
    g_cb_post_state_restore.emplace_back(std::move(fn));
}

void run_cb_post_state_restore(Client* cl)
{
    if (g_cb_post_state_restore.size())
    {
        for (const auto& fn : g_cb_post_state_restore)
        {
            fn(cl);
        }
    }
}

void add_cb_shutdown(std::function<void()>&& fn)
{
    g_cb_shutdown.emplace_back(std::move(fn));
}

void run_cb_shutdown()
{
    if (g_cb_shutdown.size())
    {
        for (const auto& fn : g_cb_shutdown)
        {
            fn();
        }
    }
}

void cleanup_callbacks()
{
    g_cb_post_state_restore.clear();
}

}
