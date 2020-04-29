/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
 */
 
#include "dfplex.hpp"
#include "staticserver.hpp"

#include <stdint.h>
#include <iostream>
#include <map>
#include <vector>
#include <list>
#include <cassert>

#include "tinythread.h"

#include "MemAccess.h"
#include "PluginManager.h"
#include "modules/EventManager.h"
#include "modules/MapCache.h"
#include "modules/Gui.h"
#include "modules/World.h"
#include "modules/Screen.h"
#include "modules/Units.h"
#include "df/announcement_flags.h"
#include "df/announcements.h"
#include "df/building.h"
#include "df/buildings_other_id.h"
#include "df/d_init.h"
#include "df/enabler.h"
#include "df/graphic.h"
#include "df/historical_figure.h"
#include "df/items_other_id.h"
#include "df/renderer.h"
#include "df/report.h"
#include "df/squad_position.h"
#include "df/squad.h"
#include "df/ui_build_selector.h"
#include "df/ui_sidebar_menus.h"
#include "df/ui_unit_view_mode.h"
#include "df/unit.h"
#include "df/viewscreen_createquotast.h"
#include "df/viewscreen_customize_unitst.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/viewscreen_loadgamest.h"
#include "df/viewscreen_meetingst.h"
#include "df/viewscreen_movieplayerst.h"
#include "df/viewscreen_optionst.h"
#include "df/viewscreen_textviewerst.h"
#include "df/viewscreen_titlest.h"
#include "df/viewscreen_unitst.h"
#include "df/viewscreen.h"
#include "df/world.h"

#include "Client.hpp"
#include "server.hpp"
#include "input.hpp"
#include "command.hpp"
#include "hackutil.hpp"
#include "keymap.hpp"
#include "config.hpp"
#include "screenbuf.hpp"
#include "state.hpp"

using namespace DFHack;
using namespace df::enums;
using df::global::world;
using std::string;
using std::vector;
using df::global::gps;
using df::global::init;

using df::global::enabler;
using df::renderer;

static tthread::thread * wsthread;
static tthread::thread * staticserver_thread;
tthread::fast_mutex dfplex_mutex;

// plex state
static bool in_df_mode = false;
bool plexing = false;
static bool uniplexing_requested = false;
bool global_pause = true; // game state should be paused.
static size_t current_user_state_index = 0;
color_ostream* _out = nullptr;
static bool server_debug_out = false;
static bool single_step_requested = false;

int32_t frames_elapsed = 0;

DFHACK_PLUGIN("dfplex");
DFHACK_PLUGIN_IS_ENABLED(enabled);

static void dfplex_update();

// returns true if any client requires pause,
// or if the global state is paused.
//
// NOTE: this function ignores the value of df's pause variable
bool is_paused()
{
    if (global_pause) return true;
    if (PAUSE_BEHAVIOUR != PauseBehaviour::ALWAYS) return false;
    if (plexing)
    {
        size_t i = 0;
        for (Client* client = get_client(i); client; i++, client = get_client(i))
        {
            UIState& ui = client->ui;
            if (ui.m_pause_required) return true;
        }
    }
    
    return false;
}

void apply_key(const KeyEvent& match, Client* cl, bool raw)
{
    // special keys
    if (MULTIPLEXKEY != 0 && (match.type == type_unicode || match.type == type_key) && match.unicode == MULTIPLEXKEY)
    {
        // enter uniplex mode.
        if (uniplexing_requested)
        {
            // returning to plex mode entails restoring to root.
            
            // for some reason this causes the game to crash..?
            if (!defer_return_to_root())
            {
                // failed to return to root; don't do anything.
                return;
            }
        }
        uniplexing_requested ^= true;
        if (uniplexing_requested)
        {
            plexing = false;
        }
        if (uniplexing_requested && cl)
        {
            // returning to the state they were in before uniplexing
            // may be confusing for the client who requested uniplexing,
            // so we erase their state.
            cl->ui.reset();
        }
        return;
    }
    if (cl && DEBUGKEY != 0 &&  (match.type == type_unicode || match.type == type_key) && match.unicode == DEBUGKEY)
    {
        // toggle debug mode
        cl->m_debug_enabled ^= true;
    }
    if (SERVERDEBUGKEY != 0 &&  (match.type == type_unicode || match.type == type_key) && match.unicode == SERVERDEBUGKEY)
    {
        // toggle debug mode
        server_debug_out ^= true;
        *_out << "debug output " << server_debug_out << endl;
    }
    if (is_at_root() && !raw && cl)
    {
        // jump to client position
        int32_t client_id = cl->ui.m_client_screen_cycle;
        if (NEXT_CLIENT_POS_KEY != 0 &&  (match.type == type_unicode || match.type == type_key) && match.unicode == NEXT_CLIENT_POS_KEY)
            client_id++;
        if (PREV_CLIENT_POS_KEY != 0 &&  (match.type == type_unicode || match.type == type_key) && match.unicode == PREV_CLIENT_POS_KEY)
            client_id++;
        if (client_id != cl->ui.m_client_screen_cycle)
        {
            cl->ui.m_stored_viewcoord_skip = true;
            cl->ui.m_client_screen_cycle = client_id;
            client_id %= get_client_count();
            Client* dst = get_client(client_id);
            if (cl->ui.m_stored_camera_return)
            {
                dst = cl;
                cl->ui.m_stored_camera_return = false;
            }
            if (dst->ui.m_viewcoord_set && dst->ui.m_viewcoord.x >= 0)
            {
                Gui::setViewCoords(dst->ui.m_stored_viewcoord.x, dst->ui.m_stored_viewcoord.y, dst->ui.m_stored_viewcoord.z);
            }
        }
    }
    
    std::set<df::interface_key> keys;
    if (match.type == type_key && match.unicode != 0){
        // this event match contains both sdl and unicode data, match both
        KeyEvent sdlMatch = match;
        sdlMatch.unicode = 0;
        keys  = keybindings.toInterfaceKey(sdlMatch);

        KeyEvent unicodeMatch;
        unicodeMatch.unicode = match.unicode;
        unicodeMatch.type = type_unicode;
        unicodeMatch.mod = 0;
        std::set<df::interface_key> unicodeKeys = keybindings.toInterfaceKey(unicodeMatch);
        keys.insert(unicodeKeys.begin(), unicodeKeys.end());
    }else {
        keys  = keybindings.toInterfaceKey(match);
    }

    // apply commands until apply_command returns true
    // (i.e. until the key does something, in particular until the viewscreen changes).
    // (in vanilla DF, commands from a single keystroke are only
    // supposed to be applied to a single viewscreen.)
    if (server_debug_out)
    {
        *_out << "keypress: " << match << endl;
        *_out << keybindings.getCommandNames(keys) << endl;
    }
    
    // single-stepping has to be handled specially.
    if (contains(keys, df::enums::interface_key::D_ONESTEP) && (is_at_root() || get_current_menu_id() == "dwarfmode/Squads"))
    {
        single_step_requested = true;
        keys.erase(df::enums::interface_key::D_ONESTEP);
    }
    
    apply_command(keys, cl, raw);
}

void apply_keys(Client* client, bool raw)
{
    while (!client->keyqueue.empty())
    {
        const KeyEvent& match = client->keyqueue.front();
        
        apply_key(match, client, raw);
        
        client->keyqueue.pop();
    }
}

static void update_uniplexing()
{
    df::viewscreen* vs;
    virtual_identity* id;
    (void)id;
    UPDATE_VS(vs, id);
    
    // get key inputs and update all clients' state.
    size_t i = 0;
    for (Client* client = get_client(i); client; i++, client = get_client(i))
    {
        apply_keys(client, true);
        
        if (plexing) break;
    }
    
    // set info message
    i = 0;
    menu_id menu_id = get_current_menu_id();
    std::string focus_string = Gui::getFocusString(vs);
    bool menu_modified = (menu_id != focus_string);
    std::string info_message = 
        std::string("(UP) ")
        + (menu_modified ? "[" : "") + focus_string + (menu_modified ? "]" : "")
        + " +" + std::to_string(get_vs_depth(vs));
    for (Client* client = get_client(i); client; i++, client = get_client(i))
    {
        // (info) UP -- uniplexing
        client->info_message = info_message;
    }
    
    UPDATE_VS(vs, id);
    
    if (server_debug_out)
    {
        if (df::global::ui_sidebar_menus && id == &df::viewscreen_dwarfmodest::_identity)
        {
            df::viewscreen_dwarfmodest* vs_dwarf = static_cast<df::viewscreen_dwarfmodest*>(vs);
            auto& menu = *df::global::ui_sidebar_menus;
            /**_out << "sidebar menu:" << endl;
            *_out << "  ui_look_cursor: " << *df::global::ui_look_cursor << endl;
            *_out << "  unit_labors_sidemenu: " << vs_dwarf->unit_labors_sidemenu.size() << endl;
            *_out << "  unit_labors_sidemenu_uplevel: " << vs_dwarf->unit_labors_sidemenu_uplevel.size() << endl;
            *_out << "  unit_labors_sidemenu_uplevel_idx: " << vs_dwarf->unit_labors_sidemenu_uplevel_idx << endl;
            *_out << "  sideSubmenu: " << (int32_t)vs_dwarf->sideSubmenu << endl;*/
        }
    }
}

// returns true if the plex completed.
static bool update_multiplexing(Client* client)
{
    df::viewscreen* vs;
    virtual_identity* id;
    (void)id;
    UPDATE_VS(vs, id);
    
    if (client)
    {
        UIState& ui = client->ui;
        
        // restore UI state for player
        RestoreResult result = restore_state(client);
        
        // apply new actions from user.
        switch(result)
        {
        case RestoreResult::SUCCESS:
            
            UPDATE_VS(vs, id);
            
            if (restore_state_error.length())
            {
                _out->color(COLOR_YELLOW);
                *_out << "Success with warning for user " << current_user_state_index << " on screen " << get_current_menu_id() << std::endl;
                *_out << restore_state_error << endl;
                _out->color(COLOR_RESET);
            }
            
            // update screen except if it would cause a tick.
            if (!is_realtime_dwarf_menu())
            {
                vs->logic();
            }
            
            apply_keys(client, false);
            
            deferred_state_restore(client);
        
            // obtain and store new state.
            capture_post_state(client);
            
            // (info) MP -- multiplexing
            {
                UPDATE_VS(vs, id);
                size_t keys_c = ui.m_restore_keys.size();
                menu_id menu_id = get_current_menu_id();
                std::string focus_string = Gui::getFocusString(vs);
                bool menu_modified = (menu_id != focus_string);
                client->info_message =
                    std::string("(MP) ")
                    + (menu_modified ? "[" : "") + focus_string + (menu_modified ? "]" : "")
                    + " +" + std::to_string(get_vs_depth(vs))
                    + " (" + std::to_string(keys_c) + ((keys_c == 1) ? " key)" : " keys)")
                    + ((!global_pause && is_paused()) ? " [resume pending]" : "");
                if (client->m_debug_enabled)
                {
                    client->m_debug_info = "(Press \"|\" to close.)\n";
                    client->m_debug_info += "\"" + get_current_menu_id() + "\"\n";
                    client->m_debug_info += ui.debug_trace();
                }
                else
                {
                    client->m_debug_info = "";
                }
            }
            return true;
            
        case RestoreResult::FAIL:
            UPDATE_VS(vs, id);
            _out->color(COLOR_RED);
            *_out << "Error for user " << current_user_state_index << " on screen " << get_current_menu_id() << std::endl;
            *_out << restore_state_error << endl;
            _out->color(COLOR_RESET);
            break;
            
        case RestoreResult::ABORT_PLEX:
        default:
            // stop plexing.
            plexing = false;
            global_pause = true;
            World::SetPauseState(global_pause);
            break;
        }
        
        deferred_state_restore(client);
    }
    
    return false;
}

// handles things like announcements, screens popping up between frames.
void check_events()
{
    df::viewscreen* vs;
    virtual_identity* id;
    UPDATE_VS(vs, id);
    
    // as a default, plex this frame.
    if (!uniplexing_requested)
    {
        plexing = true;
    }
    
    // temporarily enable uniplex mode if we are not at the root or
    // if an announcement is visible.
    if (plexing)
    {
        bool temp_uniplex = false;
        if (!is_at_root())
        {
            temp_uniplex = true;
        }
        if (!df::global::world->status.popups.empty())
        {
            temp_uniplex = true;
        }
        if (temp_uniplex)
        {
            // pause and enable uniplexing until players disable it.
            plexing = false;
            global_pause = true;
            World::SetPauseState(global_pause);
        }
    }
    
    // check if uniplexing was requested.
    if (uniplexing_requested)
    {
        plexing = false;
    }
    
    // automatically disable uniplex mode sometimes
    // FIXME spaghetti / not always expected behaviour...
    if (id == &df::viewscreen_dwarfmodest::_identity && !uniplexing_requested)
    {
        bool start_plexing = !in_df_mode;
        start_plexing |= (!plexing && is_at_root());
        if (start_plexing)
        {
            // entering df mode.
            // automatically start plexing when entering the game for the first time.
            plexing = true;
            
            // reset some ui state
            size_t i = 0;
            for (Client* client = get_client(i); client; i++, client = get_client(i))
            {
                client->ui.reset();
            }
            
            // pause, for convenience
            global_pause = true;
            World::SetPauseState(global_pause);
        }
        in_df_mode = true;
    }
    
    // automatically enable uniplexing in options and title screen
    if (!is_dwarf_mode())
    {
        in_df_mode = false;
        plexing = false;
        // when re-entering dwarf mode, plex right away.
        uniplexing_requested = false;
    }
    
    // check if traders arrived, put everyone into trading screen (but multiplexed!)
    // (TODO)
}

void dfplex_update()
{
    if (!_out) return;
    
    dfplex_mutex.lock();
    //CoreSuspender suspend;
    
    df::viewscreen* vs;
    virtual_identity* id;
    (void)id;
    UPDATE_VS(vs, id);
    
    if (!in_df_mode)
    {
        //#define SKIP_INTRO
        #ifdef SKIP_INTRO
        if (id == &df::viewscreen_movieplayerst::_identity)
        {
            vs->feed_key(interface_key::LEAVESCREEN);
        }
        if (id == &df::viewscreen_titlest::_identity || id == &df::viewscreen_loadgamest::_identity)
        {
            vs->feed_key(interface_key::SELECT);
        }
        #endif
    }
    
    // sometimes the game must enable/disable uniplex mode on its own accord.
    // this handles that.
    check_events();
    
    size_t clients_count = get_client_count();
    if (clients_count == 0)
    {
        // pause game when nobody is playing.
        World::SetPauseState(true);
    }
    else
    {        
        // multiplexing
        if (plexing)
        {
            // if we started this frame with to-be-dismissed screens on the stack, skip this update.
            // (we need df to dismiss these properly, can't interrupt it.)
            if (Screen::isDismissed(Gui::getCurViewscreen()))
            {
                // apply some pressure on DF to get back to the root screen.
                if (!defer_return_to_root())
                {
                    // failed to do that. Switch to uniplex mode.
                    uniplexing_requested = true;
                    plexing = false;
                }
            }
            else
            {
                for (current_user_state_index = 0; current_user_state_index < clients_count; ++current_user_state_index)
                {
                    Client* client = get_client(current_user_state_index);
                    if (!client) continue; // paranoia
                    
                    // user works with global pause state
                    World::SetPauseState(global_pause);
                    
                    set_size(client->desired_dimx, client->desired_dimy);
                    
                    if (update_multiplexing(client))
                    {
                        // transfer screen to this client
                        perform_render();
                        scrape_screenbuf(client);
                        transfer_screenbuf_client(client);
                    }
                    
                    restore_size();
                    
                    if (plexing) return_to_root();
                    
                    global_pause = World::ReadPauseState();
                    
                    if (!plexing) break;
                }
            }
        }
        
        // multiplex mode can cause a switch back to plexing, so
        // we do not use an "else" here.
        
        // uniplexing
        if (!plexing)
        {
            set_size(gps->dimx, gps->dimy);
            update_uniplexing();
            
            if (!plexing)
            {
                perform_render();
                transfer_screenbuf_to_all();
            }
            
            global_pause = World::ReadPauseState();
        }
        
        // decide DF's paused status for the remainder of the frame.
        World::SetPauseState(is_paused());
        
        // take a single step
        if (single_step_requested && plexing && is_at_root() && is_paused())
        {
            UPDATE_VS(vs, id);
            *_out << "single_step applied" << endl;
            vs->feed_key(df::enums::interface_key::D_ONESTEP);
        }
        single_step_requested = false;
    }
    
    frames_elapsed++;
    dfplex_mutex.unlock();
}

// when an announcement/event occurs
void on_report(color_ostream &out, void* v)
{
    dfplex_mutex.lock();
    df::report* report = df::report::find((intptr_t)v);
    df::announcement_flags flags = df::global::d_init->announcements.flags[report->type];
    
    // respect pause flags
    if (flags.bits.PAUSE)
    {
        global_pause = true;
    }
    
    // this announcement recenters the camera
    if (flags.bits.RECENTER)
    {
        if (plexing && report->pos.x >= 0 && report->pos.y >= 0)
        {
            // every player looks here now.
            for (size_t i = 0; i <= get_client_count(); ++i)
            {
                // handle cl last, to draw its cursor topmost.
                Client* client = get_client(i);
                if (client)
                {
                    auto dims = DFHack::Gui::getDwarfmodeViewDims();
                    
                    client->ui.m_viewcoord_set = true;
                    // go to coord - map dimensions/2 to center.
                    client->ui.m_viewcoord = report->pos - Coord{ (dims.map_x2 - dims.map_x1) / 2, (dims.map_y2 - dims.map_y1) / 2, 0};
                    client->ui.m_viewcoord.z = report->pos.z; // why is this needed?
                    if (client->ui.m_viewcoord.x < 0) client->ui.m_viewcoord.x = 0;
                    if (client->ui.m_viewcoord.y < 0) client->ui.m_viewcoord.y = 0;
                    client->ui.m_stored_camera_return = true;
                }
            }
        }
    }
    dfplex_mutex.unlock();
}

static void enable()
{
    if (enabled) return;
    enabled = true;
    
    EventManager::registerListener(EventManager::EventType::REPORT, 
    EventManager::EventHandler {
        on_report, 0
    }, plugin_self);
    
    hook_renderer();

    if (PORT != 0)
    {
        wsthread = new tthread::thread(wsthreadmain, _out);
    }
    if (STATICPORT != 0)
    {
        staticserver_thread = new tthread::thread(init_static, nullptr);
    }
}

static void disable()
{
    if (!enabled) return;
    enabled = false;
    
    unhook_renderer();
}

DFhackCExport command_result plugin_init(color_ostream &out, vector <PluginCommand> &commands)
{
    assert(!enabled);
    
    memset(screenbuf, 0, sizeof(screenbuf));
    
    _out = &out;

    if (!keybindings.loadKeyBindings(out,"data/init/interface.txt")){
        out.color(COLOR_RED);
        out << "Error: For dfplex, could not load keybindings" << std::endl;
        out.color(COLOR_RESET);
        return CR_OK;
    }

    load_config();
    
    if (command_init())
    {
        out.color(COLOR_RED);
        out << "Error: dfplex could not open command config" << std::endl;
        out.color(COLOR_RESET);
        return CR_OK;
    }
    
    return CR_OK;
}

DFhackCExport command_result plugin_onupdate(color_ostream &out)
{
    dfplex_mutex.lock();
    _out = &out;
    dfplex_mutex.unlock();
    
    if (!enabled) return CR_OK;
    
    dfplex_update();
    return CR_OK;
}

DFhackCExport command_result plugin_enable(color_ostream &out, bool to_enable)
{
    dfplex_mutex.lock();
    _out = &out;
    
    if (to_enable)
        enable();
    else
        disable();
    dfplex_mutex.unlock();
        
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown(color_ostream &out)
{
    dfplex_mutex.lock();
    _out = &out;
    dfplex_mutex.unlock();
    
    if (!enabled) return CR_OK;
    
    dfplex_mutex.lock();
    disable();
    dfplex_mutex.unlock();

    return CR_OK;
}
