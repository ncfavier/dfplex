#include "dfplex.hpp"
#include "Client.hpp"
#include "callbacks.hpp"

#include "Core.h"
#include "modules/Gui.h"
#include "DataFuncs.h"
#include "PluginManager.h"
#include <lua.h>

namespace
{
    static int g_tab_idx;
    
    // [-0, +0, -]
    void init_lua_reg(lua_State* L)
    {
        static bool init = false;
        if (!init)
        {
            lua_newtable(L); // +1
            g_tab_idx = luaL_ref(L,LUA_REGISTRYINDEX); // -1
            auto tab_idx = g_tab_idx;
            DFPlex::add_cb_shutdown(
                [L, tab_idx]()
                {
                    luaL_unref(L, LUA_REGISTRYINDEX, g_tab_idx); // 0
                    init = false;
                }
            );
            init = true;
        }
    }
    
    // inspired by https://stackoverflow.com/a/31952046
    // [-1, +0, -]
    int lua_store_reg(lua_State* L)
    {
        init_lua_reg(L);
        
        // push table.
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_tab_idx); // + 1
        
        // table should be before arg.
        lua_rotate(L, -2, 1); // 0
        
        // store arg in table.
        int t = luaL_ref(L, -2); // -1
        
        // pop table.
        lua_pop(L, 1); // -1
        
        // return reference to arg.
        return t;
    }
    
    // [-0, +1, -]
    void lua_load_reg(lua_State* L, int index)
    {
        // retrieve table.
        lua_rawgeti(L,LUA_REGISTRYINDEX,g_tab_idx); // +1
        
        // retrieve value.
        lua_rawgeti(L, -1, index); // +1
        
        // we want to pop the table.
        lua_rotate(L, -2, 1); // 0
        lua_pop(L, 1);
    }
    
    // returns 0 if no client found, positive unique identifier otherwise.
    uint32_t lua_get_client_count()
    {
        return get_client_count();
    }
    
    client_long_id_t lua_get_client_id_by_index(uint32_t index)
    {
        Client* c = get_client(index);
        if (c)
        {
            return c->id->long_id;
        }
        
        return 0;
    }
    
    int lua_get_client_nick(lua_State* L)
    {
        client_long_id_t id = lua_tointeger(L, -1); // 0
        lua_pop(L, 1); // -1
        Client* client = get_client_by_id(id);
        if (client)
        {
            lua_pushstring(L, client->id->nick.c_str());
        }
        else
        {
            lua_pushnil(L);
        }
        return 1;
    }
    
    int lua_get_client_cursorcoord(lua_State* L)
    {
        int id = lua_tointeger(L, -1);
        lua_pop(L, 1);
        Client* cl = get_client_by_id(id);
        
        int x = -30000, y = -30000, z = -30000;
        
        if (cl)
        {
            if (cl->ui.m_cursorcoord_set)
            {
                x = cl->ui.m_cursorcoord.x;
                y = cl->ui.m_cursorcoord.y;
                z = cl->ui.m_cursorcoord.z;
            }
        }
        
        lua_pushinteger(L, x);
        lua_pushinteger(L, y);
        lua_pushinteger(L, z);
        
        return 3;
    }
    
    void lua_set_client_cursorcoord(client_long_id_t id, int x, int y, int z)
    {
        Client* cl = get_client_by_id(id);
        if (cl)
        {
            cl->ui.m_cursorcoord_set = true;
            cl->ui.m_cursorcoord.x = x;
            cl->ui.m_cursorcoord.y = y;
            cl->ui.m_cursorcoord.z = z;
            DFHack::Gui::setCursorCoords(x, y, z);
        }
    }
    
    void lua_set_client_viewcoord(client_long_id_t id, int x, int y, int z)
    {
        Client* cl = get_client_by_id(id);
        if (cl)
        {
            cl->ui.m_viewcoord_set = true;
            cl->ui.m_viewcoord.x = x;
            cl->ui.m_viewcoord.y = y;
            cl->ui.m_viewcoord.z = z;
            DFHack::Gui::setViewCoords(x, y, z);
        }
    }
    
    int lua_get_client_viewcoord(lua_State* L)
    {
        int id = lua_tointeger(L, -1);
        lua_pop(L, 1);
        Client* cl = get_client_by_id(id);
        
        int x = -30000, y = -30000, z = -30000;
        
        if (cl)
        {
            if (cl->ui.m_viewcoord_set)
            {
                x = cl->ui.m_viewcoord.x;
                y = cl->ui.m_viewcoord.y;
                z = cl->ui.m_viewcoord.z;
            }
        }
        
        lua_pushinteger(L, x);
        lua_pushinteger(L, y);
        lua_pushinteger(L, z);
        
        return 3;
    }
    
    int lua_get_current_menu_id(lua_State* L)
    {
        std::string s { get_current_menu_id() };
        lua_pushstring(L, s.c_str());
        return 1;
    }
    
    int lua_register_cb_post_state_restore(lua_State* L)
    {
        // pop & store callback function in registry.
        int index = lua_store_reg(L);
       
        DFPlex::add_cb_post_state_restore(
            [L, index](Client* cl)
            {
                client_long_id_t id = (cl)
                    ? cl->id->long_id
                    : 0;
                    
                // push callback function onto stack
                lua_load_reg(L, index); // +1
                
                // push client index onto stack (arg)
                lua_pushinteger(L, id); // +1
                
                // call callback function.
                lua_call(L, 1, 0); // -2
            }
        );
       
        // return value
        lua_pushnil(L);
        return 1;
    }
}

#undef DFHACK_LUA_FUNCTION
#define DFHACK_LUA_FUNCTION(name) { #name, df::wrap_function(lua_##name, true) }

#undef DFHACK_LUA_COMMAND
#define DFHACK_LUA_COMMAND(name) { #name, lua_##name }

DFHACK_PLUGIN_LUA_FUNCTIONS {
    DFHACK_LUA_FUNCTION(get_client_count),
    DFHACK_LUA_FUNCTION(get_client_id_by_index),
    DFHACK_LUA_FUNCTION(set_client_cursorcoord),
    DFHACK_LUA_FUNCTION(set_client_viewcoord),
    DFHACK_LUA_END
};

DFHACK_PLUGIN_LUA_COMMANDS {
    DFHACK_LUA_COMMAND(get_current_menu_id),
    DFHACK_LUA_COMMAND(get_client_nick),
    DFHACK_LUA_COMMAND(register_cb_post_state_restore),
    DFHACK_LUA_COMMAND(get_client_cursorcoord),
    DFHACK_LUA_COMMAND(get_client_viewcoord),
    DFHACK_LUA_END
};