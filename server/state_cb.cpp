/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
 */
 
/*
    This file contains helper functions which can be attached as callbacks
    to RestoreKeys to affect how state is restored.
*/
 
#include "Client.hpp"
#include "command.hpp"
#include "config.hpp"
#include "dfplex.hpp"
#include "hackutil.hpp"
#include "input.hpp"
#include "keymap.hpp"
#include "screenbuf.hpp"
#include "server.hpp"
#include "state_cb.hpp"

#include <stdint.h>
#include <iostream>
#include <map>
#include <vector>
#include <list>
#include <cassert>

#include "tinythread.h"
#include "MemAccess.h"
#include "modules/EventManager.h"
#include "modules/Gui.h"
#include "modules/MapCache.h"
#include "modules/Screen.h"
#include "modules/Units.h"
#include "modules/World.h"
#include "PluginManager.h"

#include "df/announcement_flags.h"
#include "df/announcements.h"
#include "df/building.h"
#include "df/buildings_other_id.h"
#include "df/d_init.h"
#include "df/enabler.h"
#include "df/graphic.h"
#include "df/historical_figure.h"
#include "df/interfacest.h"
#include "df/items_other_id.h"
#include "df/renderer.h"
#include "df/report.h"
#include "df/squad_position.h"
#include "df/squad.h"
#include "df/ui_build_selector.h"
#include "df/ui_sidebar_menus.h"
#include "df/ui_unit_view_mode.h"
#include "df/unit.h"
#include "df/viewscreen_announcelistst.h"
#include "df/viewscreen_createquotast.h"
#include "df/viewscreen_customize_unitst.h"
#include "df/viewscreen_civlistst.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/viewscreen_loadgamest.h"
#include "df/viewscreen_meetingst.h"
#include "df/viewscreen_movieplayerst.h"
#include "df/viewscreen_optionst.h"
#include "df/viewscreen_reportlistst.h"
#include "df/viewscreen_textviewerst.h"
#include "df/viewscreen_tradelistst.h"
#include "df/viewscreen_titlest.h"
#include "df/viewscreen_unitst.h"
#include "df/viewscreen.h"
#include "df/world.h"

using namespace DFHack;
using namespace df::enums;
using df::global::world;
using std::string;
using std::vector;
using df::global::gps;
using df::global::init;

using df::global::enabler;
using df::renderer;

int suppress_sidebar_refresh(Client* client)
{
    client->ui.m_suppress_sidebar_refresh = true;
    return 0;
}

// helper function for restore_state.
int restore_cursor(Client* client)
{
    UIState& ui = client->ui;
    
    // sets viewcoords
    if (ui.m_viewcoord_set)
    {
        Gui::setViewCoords(ui.m_viewcoord.x, ui.m_viewcoord.y, ui.m_viewcoord.z);
    }
    if (ui.m_following_client && ui.m_client_screen_cycle.get())
    {
        Client* dst = get_client(ui.m_client_screen_cycle.get());
        if (dst)
        {
            center_view_on_coord(dst->ui.m_stored_viewcoord.operator+(
                {dst->ui.m_map_dimx/2, dst->ui.m_map_dimy/2, 0})
            );
        }
    }
    
    // sets cursor coords
    if (ui.m_cursorcoord_set)
    {
        Gui::setCursorCoords(ui.m_cursorcoord.x, ui.m_cursorcoord.y, ui.m_cursorcoord.z);
    }
    if (ui.m_designationcoord_set)
    {
        Gui::setDesignationCoords(ui.m_designationcoord.x, ui.m_designationcoord.y, ui.m_designationcoord.z);
    }
    if (ui.m_squadcoord_start_set)
    {
        df::global::ui->squads.rect_start.x = ui.m_squadcoord_start.x;
        df::global::ui->squads.rect_start.y = ui.m_squadcoord_start.y;
        df::global::ui->squads.rect_start.z = ui.m_squadcoord_start.z;
    }
    if (ui.m_burrowcoord_set)
    {
        df::global::ui->burrows.rect_start.x = ui.m_burrowcoord.x;
        df::global::ui->burrows.rect_start.y = ui.m_burrowcoord.y;
        df::global::ui->burrows.rect_start.z = ui.m_burrowcoord.z;
    }
    df::global::ui->burrows.brush_erasing = ui.m_brush_erasing;
    
    return 0;
}

restore_state_cb_t produce_restore_cb_restore_cursor()
{
    Coord c;
    Gui::getCursorCoords(c.x, c.y, c.z);
    return [c](Client* client) -> int
    {
        Gui::setCursorCoords(c.x, c.y, c.z);
        return 0;
    };
}

int restore_squads_state(Client* client)
{
    UIState& ui = client->ui;
    
    auto& squads = df::global::ui->squads;
    auto& ui_squads = ui.m_squads;
    
    // need to clear this on entry.
    squads.in_kill_rect = false;
    squads.in_kill_order = false;
    squads.in_kill_list = false;
    squads.in_move_order = false;
    
    // selected individuals
    squads.in_select_indiv = ui_squads.in_select_indiv;
    
    // sanitize
    squads.indiv_selected.clear();
    for (int32_t figure_id : ui_squads.indiv_selected)
    {
        df::historical_figure* figure = df::historical_figure::find(figure_id);
        if (!figure) continue;
        df::unit* unit = df::unit::find(figure->unit_id);
        if (!unit) continue;
        
        // ensure unit still part of a squad.
        bool found_squad = false;
        for (df::squad* squad : squads.list)
        {
            if (squad && squad->id == unit->military.squad_id)
            {
                found_squad = true;
                break;
            }
        }
        if (!found_squad) continue;
        
        // ensure unit is assigned to a squad position.
        df::squad* squad = df::squad::find(unit->military.squad_id);
        if (!squad) continue;
        
        bool found_position = false;
        for (df::squad_position* squad_position : squad->positions)
        {
            if (squad_position && squad_position->occupant == figure_id)
            {
                found_position = true;
            }
        }
        if (!found_position) continue;
        
        // we were unable to prove that this figure is not valid to select,
        // so we will allow this figure to remain selected.
        squads.indiv_selected.push_back(figure_id);
    }
    
    // selected squads
    for (size_t i = 0; i < squads.list.size() && i < squads.sel_squads.size(); ++i)
    {
        squads.sel_squads.at(i) = false;
        
        df::squad* squad = squads.list.at(i);
        if (!squad) continue;
        
        squads.sel_squads.at(i) =
            (
                std::find(ui_squads.squad_selected.begin(), ui_squads.squad_selected.end(), squad->id)
                != ui_squads.squad_selected.end()
            );
    }
    
    return 0;
}

// returns 1 on error.
int restore_unit_view_state(Client* client)
{
    using namespace df::enums::interface_key;
    
    df::viewscreen* vs;
    virtual_identity* id;
    (void)id;
    UPDATE_VS(vs, id);
    
    UIState& ui = client->ui;
    df::global::ui_unit_view_mode->value = ui.m_unit_view_mode;
    df::global::ui_sidebar_menus->show_combat = ui.m_show_combat;
    df::global::ui_sidebar_menus->show_labor = ui.m_show_labor;
    df::global::ui_sidebar_menus->show_misc = ui.m_show_misc;

    // set df::global::ui_selected_unit
    if (df::unit* unit = df::unit::find(ui.m_view_unit))
    {
        // go to unit's position.
        Coord pos = unit->pos;
        Gui::setCursorCoords(pos.x, pos.y, pos.z);
        Gui::refreshSidebar();
        
        // rapidly tap UNITVIEW_NEXT until the unit we desire is found.
        vs->feed_key(UNITVIEW_NEXT);
        int32_t unit_sel_start = *df::global::ui_selected_unit;
        bool success;
        while (true)
        {
            vs->feed_key(UNITVIEW_NEXT);
            if (df::unit* unit_selected = vector_get(world->units.active, *df::global::ui_selected_unit))
            {
                if (unit_selected->id == unit->id)
                {
                    success = true;
                    break;
                }
            }
            if (unit_sel_start == *df::global::ui_selected_unit)
            {
                success = false;
                break;
            }
        }
        
        if (!success)
        {
            // return to the stored cursor position.
            restore_cursor(client);
            Gui::refreshSidebar();
            ui.m_view_unit = -1;
            return 1;
        }
        else
        {
            ui.m_defer_restore_cursor = true;
            ui.m_suppress_sidebar_refresh = true;
            
            // restore labour menu position
            if (ui.m_unit_view_mode == df::ui_unit_view_mode::PrefLabor)
            {
                vs->feed_key(UNITVIEW_PRF);
                vs->feed_key(UNITVIEW_PRF_PROF);
                
                // set scroll position
                if (ui.m_view_unit_labor_submenu >= 0)
                {
                    for (int32_t i = 0; i < ui.m_view_unit_labor_submenu; ++i)
                    {
                        vs->feed_key(SECONDSCROLL_DOWN);
                    }
                    vs->feed_key(SELECT);
                }
                for (int32_t i = 0; i < ui.m_view_unit_labor_scroll; ++i)
                {
                    vs->feed_key(SECONDSCROLL_DOWN);
                }
            }
            return 0;
        }
    }
    return 0;
}