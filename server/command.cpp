#/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2020 white-rabbit, ISC license
*/

#include "command.hpp"
#include "hackutil.hpp"
#include "dfplex.hpp"
#include "parse_config.hpp"
#include "config.hpp"

#include "DataDefs.h"

#include "df/ui_unit_view_mode.h"
#include "df/viewscreen_announcelistst.h"
#include "df/viewscreen_buildinglistst.h"
#include "df/viewscreen_civlistst.h"
#include "df/viewscreen_createquotast.h"
#include "df/viewscreen_customize_unitst.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/viewscreen_itemst.h"
#include "df/viewscreen_joblistst.h"
#include "df/viewscreen_jobmanagementst.h"
#include "df/viewscreen_layer_assigntradest.h"
#include "df/viewscreen_layer_militaryst.h"
#include "df/viewscreen_layer_noblelistst.h"
#include "df/viewscreen_layer_squad_schedulest.h"
#include "df/viewscreen_layer_stockpilest.h"
#include "df/viewscreen_layer_unit_relationshipst.h"
#include "df/viewscreen_loadgamest.h"
#include "df/viewscreen_movieplayerst.h"
#include "df/viewscreen_optionst.h"
#include "df/viewscreen_overallstatusst.h"
#include "df/viewscreen_petitionsst.h"
#include "df/viewscreen_reportlistst.h"
#include "df/viewscreen_textviewerst.h"
#include "df/viewscreen_titlest.h"
#include "df/viewscreen_tradelistst.h"
#include "df/viewscreen_treasurelistst.h"
#include "df/viewscreen_unitlistst.h"
#include "df/viewscreen_unitst.h"
#include "df/viewscreen_workquota_conditionst.h"
#include "df/viewscreen_workquota_detailsst.h"
#include "df/viewscreen.h"
#include "df/world.h"

#include <memory>

using namespace df;
using namespace DFHack;
using namespace df::enums::interface_key;

class command_node
{
public:
    // returns true if processed.
    virtual bool process(Client* cl, const df::interface_key key)=0;
};

class command_list : public command_node
{
public:
    std::vector<std::unique_ptr<command_node>> commands;
    virtual bool process(Client* cl, const df::interface_key key)
    {
        for (std::unique_ptr<command_node>& command : commands)
        {
            if (command->process(cl, key))
            {
                return true;
            }
        }
        return false;
    }
} g_root;

bool command_init()
{
    std::vector<config_symbol> symbols = parse_config_file("data/init/commands.txt");
    return false;
}

static bool try_shrink_keyqueue_to(Client*, size_t menu_depth, menu_id menu_id);

#define SPECIAL_CASE(case) if (false) case:

#define RUN_OVER_INKEYS(bottom, top, XX) \
    do { \
        for (const auto& _key : inkeys) \
            if (_key >= (bottom) && _key <= (top)) { \
                XX(_key); \
            } \
    } while (0)

#define STORE_TEXTENTRY() \
    RUN_OVER_INKEYS(STRING_A000, STRING_A255, savekeys.insert)

#define UNSTORE_TEXTENTRY() \
    RUN_OVER_INKEYS(STRING_A000, STRING_A255, savekeys.erase)

#define STORE_SECONDSCROLL() \
    RUN_OVER_INKEYS(SECONDSCROLL_UP, SECONDSCROLL_PAGEDOWN, savekeys.insert)

#define UNSTORE_SECONDSCROLL() \
    RUN_OVER_INKEYS(SECONDSCROLL_UP, SECONDSCROLL_PAGEDOWN, savekeys.erase)

#define STORE_STANDARDSCROLL() \
    RUN_OVER_INKEYS(STANDARDSCROLL_UP, STANDARDSCROLL_PAGEDOWN, savekeys.insert)

#define UNSTORE_STANDARDSCROLL() \
    RUN_OVER_INKEYS(STANDARDSCROLL_UP, STANDARDSCROLL_PAGEDOWN, savekeys.erase)

#define STORE_CURSORSCROLL() \
    RUN_OVER_INKEYS(CURSOR_UP, CURSOR_DOWN_Z_AUX, savekeys.insert)

#define REMOVE_CURSORSCROLL_HELPER(_key) \
    (keys.erase(_key), savekeys.erase(_key))

#define REMOVE_CURSORSCROLL() \
    RUN_OVER_INKEYS(CURSOR_UP, CURSOR_DOWN_Z_AUX, REMOVE_CURSORSCROLL_HELPER)

#define ZOOM_UNIT_ON(keyname) \
if (contains(keys, UNITJOB_ZOOM_CRE)) \
{ \
    keys.clear(); \
    vs->feed_key(UNITJOB_ZOOM_CRE); \
    ui.m_restore_keys.clear(); \
    ui.m_restore_keys.emplace_back(); \
    ui.m_restore_keys.back().m_interface_keys = { D_VIEWUNIT }; \
    ui.m_restore_keys.back().m_restore_unit_view_state = true; \
    ui.m_viewcycle = 0; \
    suppress_sidebar_refresh = true; \
    post_restore_cursor = true; \
    blockcatch = true; \
} \

// applies and saves a key.
static void apply_key(Client* cl, df::interface_key key)
{
    UIState& ui = cl->ui;
    df::viewscreen* vs;
    virtual_identity* id;
    (void)id;
    UPDATE_VS(vs, id);
    
    ui.m_restore_keys.emplace_back(key);
    vs->feed_key(key);
}

template<typename... Args>
static void apply_keys(Client* cl)
{ }

// applies and saves keys.
template<typename... Args>
static void apply_keys(Client* cl, df::interface_key key, Args... args)
{
    apply_key(cl, key);
    apply_keys(cl, args...);
}

// nudge cursor back and forth once to update sidebar.
static void cursornudge(Client* cl, bool post=true)
{
    UIState& ui = cl->ui;
    {
        // we need to make sure that the
        // cursor is in the correct spot.
        RestoreKey none;
        Gui::getCursorCoords(none.m_cursor.x, none.m_cursor.y, none.m_cursor.z);
        if (post)
        {
            none.m_post_restore_cursor = true;
        }
        else
        {
            none.m_pre_restore_cursor = true;
        }
        
        // go up and down to refresh the placement location (for material selection)
        RestoreKey up;
        up.m_interface_keys = { CURSOR_UP_Z };
        RestoreKey down;
        down.m_interface_keys = { CURSOR_DOWN_Z };
        ui.m_restore_keys.push_back(std::move(none));
        ui.m_restore_keys.push_back(std::move(up));
        ui.m_restore_keys.push_back(std::move(down));
    }
}

// parameters:
//   cl: (in) the client who is applying the keypresses
// keys: (in-out)
//       in: the new keys requested to be applied this frame
//      out: the keys which are to be applied.
// rkey: (out) what is to be immediately applied and also saved.
//       note: the logic in apply_command may choose to save additional keys.
static void apply_special_case(Client* cl, std::set<df::interface_key>& keys, RestoreKey& rkey)
{
    UIState& ui = cl->ui;
    df::viewscreen* vs;
    virtual_identity* id;
    UPDATE_VS(vs, id);
    
    menu_id mid = get_current_menu_id();
    std::string fs = Gui::getFocusString(vs);
    
    // alias
    std::set<df::interface_key>& savekeys = rkey.m_interface_keys;
    bool& _catch = rkey.m_catch;
    bool& blockcatch = rkey.m_blockcatch;
    bool& suppress_sidebar_refresh = rkey.m_suppress_sidebar_refresh;
    bool& post_restore_cursor = rkey.m_post_restore_cursor;
    bool& restore_cursor = rkey.m_pre_restore_cursor;
    bool& observe_for_autorewind = rkey.m_catch_observed_autorewind;
    menu_id& post_menu_id = rkey.m_post_menu;
    size_t& post_menu_depth = rkey.m_post_menu_depth;
    
    // save a copy of keys, as we may modify it.
    const std::set<df::interface_key> inkeys = keys;
    
    // default
    post_menu_depth = get_vs_depth(vs) + 1;
    
    assert(savekeys.empty());
    
    for (auto iter = inkeys.begin(); iter != inkeys.end(); ++iter)
    {
        switch(*iter)
        {
        // remove some keys which we don't currently support.
        case D_ONESTEP:
            // single-stepping is handled by main logic.
            keys.erase(*iter);
            break;
        case UNITVIEW_FOLLOW:
        case D_LOOK_FOLLOW:
            // TODO (feature) plex following
            keys.erase(*iter);
            break;
        case D_MILITARY_NAME_SQUAD:
        case D_MILITARY_ALERTS_NAME:
            keys.erase(*iter);
            break;
        case CHANGETAB:
        case SEC_CHANGETAB:
            // always store changetab except in dwarfmode view, where it 
            // toggles the menu.
            if (id != &df::viewscreen_dwarfmodest::_identity)
            {
                savekeys.insert(*iter);
                break;
            }
        default:
            break;
        }
    }
    
    // by default, save second-scrolling and primary-scrolling and text entry.
    STORE_TEXTENTRY();
    STORE_SECONDSCROLL();
    STORE_STANDARDSCROLL();
    if (ui.m_freeze_cursor)
    {
        REMOVE_CURSORSCROLL();
    }
    
    if (id == &df::viewscreen_dwarfmodest::_identity)
    {
        switch(df::global::ui->main.mode)
        {
        case df::ui_sidebar_mode::Default:
            // This is the "root" menu.
            
            //! RATIONALE these store a default submenu, so they need
            // an extra key applied to stabilize that default.
            if (contains(keys, D_STOCKPILES))
            {
                apply_keys(cl, D_STOCKPILES);
                savekeys = { ui.m_stockpile_mode };
                keys.clear();
            }
            if (contains(keys, D_DESIGNATE))
            {
                apply_keys(cl, D_DESIGNATE);
                savekeys = { ui.m_designate_mode };
                keys.clear();
            }
            
            //! RATIONALE needs to restore some state after applying
            if (contains(keys, D_SQUADS))
            {
                savekeys = { D_SQUADS };
                keys.clear();
                rkey.m_restore_squad_state = true;
                
                // squad menu id is funny, this is important
                rkey.m_post_menu = "*";
                rkey.m_post_menu_depth = get_vs_depth(vs);
            }
            
            //! RATIONALE these all need consistent cursor position
            if (contains(keys, D_VIEWUNIT))
            {
                rkey.m_restore_unit_view_state = true;
                ui.m_viewcycle = 0;
                
                suppress_sidebar_refresh = true;
                post_restore_cursor = true;
                
                // stop rewinds due to the unit view changing here.
                blockcatch = true;
            }
            if (contains(keys, D_BUILDJOB))
            {
                suppress_sidebar_refresh = true;
                post_restore_cursor = true;
                observe_for_autorewind = true;
                blockcatch = true;
            }
            if (contains(keys, D_LOOK))
            {
                suppress_sidebar_refresh = true;
                post_restore_cursor = true;
            }
            if (contains(keys, D_BUILDITEM))
            {
                suppress_sidebar_refresh = true;
                post_restore_cursor = true;
                blockcatch = true;
            }
            if (contains(keys, D_CIVZONE))
            {
                suppress_sidebar_refresh = true;
                post_restore_cursor = true;
                blockcatch = true;
            }
            break;
        case df::ui_sidebar_mode::Squads:
            if (get_current_menu_id() == "dwarfmode/Squads/kill")
            {
                //! RATIONALE need to save cursor position
                if (contains(keys, D_SQUADS_KILL_RECT))
                {
                    apply_key(cl, D_SQUADS_KILL_RECT);
                    cursornudge(cl);
                    keys.clear();
                }
            }
            break;
        case df::ui_sidebar_mode::Hauling:
            {
                // renaming doesn't need storing here.
                UNSTORE_TEXTENTRY();
            }
            break;
        case df::ui_sidebar_mode::Build:
            if (contains(keys, SELECT))
            {
                if (get_current_menu_id() == "dwarfmode/Build/Material/Groups")
                {
                    // error out if we don't arrive here.
                    post_menu_id = "dwarfmode/Build/Material/Groups";
                    post_menu_depth = get_vs_depth(vs);
                    savekeys.insert(SELECT);
                }
                else if (startsWith(get_current_menu_id(), "dwarfmode/Build/Position"))
                {
                    cursornudge(cl);
                    
                    // error out if we don't arrive here.
                    post_menu_id = "dwarfmode/Build/Material/Groups";
                    post_menu_depth = get_vs_depth(vs);
                    suppress_sidebar_refresh = true;
                    savekeys.insert(SELECT);
                }
            }
            for (const auto& _key : keys)
            {
                if (_key >= BUILDING_DIM_Y_UP && _key <= BUILDING_TRACK_STOP_DUMP)
                {
                    //! RATIONALE: this state is not cached by DF, so we must store it.
                    // TODO: check that all these really are uncached state.
                    // TODO: filter out ones which aren't relevant, these are quite a few.
                    
                    savekeys.insert(_key);
                }
            }
            if (get_current_menu_id() == "dwarfmode/Build/Type")
            {
                for (const auto& _key : keys)
                {
                    //! RATIONALE: unmarked submenus.
                    if (_key == HOTKEY_BUILDING_CONSTRUCTION 
                        || _key == HOTKEY_BUILDING_WORKSHOP
                        || _key == HOTKEY_BUILDING_TRAP
                        || _key == HOTKEY_BUILDING_MACHINE
                        || _key == HOTKEY_BUILDING_SIEGEENGINE
                        || _key == HOTKEY_BUILDING_FURNACE
                        || _key == HOTKEY_BUILDING_CONSTRUCTION_TRACK
                    )
                    {
                        _catch = true;
                        savekeys.insert(_key);
                    }
                }
            }
            if (contains(keys, LEAVESCREEN))
            {
                vs->feed_key(LEAVESCREEN);
                rewind_keyqueue_to_catch(cl);
                keys.clear();
            }
            break;
        case df::ui_sidebar_mode::Stockpiles:
            // possibly set the new default designation
            for (const auto& _key : keys)
            {
                if (_key >= STOCKPILE_ANIMAL && _key <= STOCKPILE_NONE && _key != STOCKPILE_CUSTOM_SETTINGS)
                {
                    ui.m_stockpile_mode = _key;
                    // This actually puts the key on the stack; it could just replace the existing
                    // stabilizing key which is used anyway.
                    // OPTIMIZE(very minor)
                    savekeys.insert(_key);
                }
            }
            break;
        case df::ui_sidebar_mode::ViewUnits:
            {
                if (contains(keys, UNITVIEW_NEXT))
                {
                    // have to restore cursor for this to work.
                    if (ui.m_cursorcoord_set)
                    {
                        Gui::setCursorCoords(ui.m_cursorcoord.x, ui.m_cursorcoord.y, ui.m_cursorcoord.z);
                        Gui::refreshSidebar();
                    }
                    ui.m_viewcycle++;
                    
                    // go to lowest-ID'd unit
                    int32_t lowest_id = -2;
                    if (df::unit* unit_selected = vector_get(df::global::world->units.active, *df::global::ui_selected_unit))
                    {
                        lowest_id = unit_selected->id;
                        while (true)
                        {
                            vs->feed_key(UNITVIEW_NEXT);
                            if (df::unit* new_unit_selected = vector_get(df::global::world->units.active, *df::global::ui_selected_unit))
                            {
                                if (new_unit_selected->id == lowest_id)
                                {
                                    break;
                                }
                                if (new_unit_selected->id < lowest_id)
                                {
                                    lowest_id = new_unit_selected->id;
                                }
                            }
                        }
                    }
                    
                    for (int32_t i = 0; i < ui.m_viewcycle; ++i)
                    {
                        vs->feed_key(UNITVIEW_NEXT);
                    }
                    keys.erase(UNITVIEW_NEXT);
                    
                    // reset ui.m_viewcycle if we return to the original unit.
                    if (df::unit* unit_selected = vector_get(df::global::world->units.active, *df::global::ui_selected_unit))
                    {
                        if (unit_selected->id == lowest_id)
                        {
                            ui.m_viewcycle = 0;
                        }
                    }
                }
                
                for (const auto& _key : inkeys)
                {
                    if (df::global::ui_unit_view_mode && df::global::ui_unit_view_mode->value == df::ui_unit_view_mode::PrefLabor)
                    {
                        // do NOT save scrolling; we store the cursor position elsewhere.
                        if (_key >= SECONDSCROLL_UP && _key <= SECONDSCROLL_PAGEDOWN)
                        {
                            vs->feed_key(_key);
                            savekeys.erase(_key);
                            keys.erase(_key);
                        }
                    }
                        
                    // abort "defer restore cursor" if the cursor moves.
                    if (_key >= CURSOR_UP && _key <= CURSOR_DOWN_Z_AUX)
                    {
                        // restore cursor and apply
                        if (ui.m_cursorcoord_set)
                        {
                            Gui::setCursorCoords(ui.m_cursorcoord.x, ui.m_cursorcoord.y, ui.m_cursorcoord.z);
                            ui.m_defer_restore_cursor = false;
                            vs->feed_key(_key);
                            keys.erase(_key);
                        }
                    }
                }
            }
            break;
        case df::ui_sidebar_mode::QueryBuilding:
            {
                //! RATIONALE these are state that need to be remembered
                if (contains(keys, RECENTER_ON_LEVER))
                {
                    savekeys.insert(RECENTER_ON_LEVER);
                }
                if (fs == "dwarfmode/QueryBuilding/Some/FarmPlot")
                {
                    if (contains(keys, BUILDJOB_FARM_SPRING))
                    {
                        savekeys = { BUILDJOB_FARM_SPRING };
                    }
                    if (contains(keys, BUILDJOB_FARM_SUMMER))
                    {
                        savekeys = { BUILDJOB_FARM_SUMMER };
                    }
                    if (contains(keys, BUILDJOB_FARM_AUTUMN))
                    {
                        savekeys = { BUILDJOB_FARM_AUTUMN };
                    }
                    if (contains(keys, BUILDJOB_FARM_WINTER))
                    {
                        savekeys = { BUILDJOB_FARM_WINTER };
                    }
                }
                if (fs == "dwarfmode/QueryBuilding/Some/Assign/Unit")
                {
                    if (contains(keys, SELECT))
                    {
                        // FIXME: this results in quitting to root; it shouldn't.
                        vs->feed_key(SELECT);
                        rewind_keyqueue_to_catch(cl);
                        keys.clear();
                    }
                }
                if (startsWith(fs, "dwarfmode/QueryBuilding/Some") && (endsWith(fs,"/Job") || endsWith(fs, "/Empty")))
                {
                    if (contains(keys, BUILDJOB_ADD))
                    {
                        _catch = true;
                        if (startsWith(fs, "dwarfmode/QueryBuilding/Some/Lever"))
                        {
                            // levers have cursor weirdness, they need to remember it.
                            restore_cursor = true;
                            rkey.m_freeze_cursor = true; // prevents cursor saving on all future frames
                            ui.m_freeze_cursor = true; // prevents cursor saving for this frame
                        }
                        savekeys = { BUILDJOB_ADD };
                        keys.clear();
                    }
                    if (contains(keys, BUILDJOB_DETAILS))
                    {
                        savekeys = { BUILDJOB_DETAILS };
                        keys.clear();
                    }
                }
                else if (get_current_menu_id() == "dwarfmode/QueryBuilding/Some/Stockpile")
                {
                    // settings
                    if (contains(keys, BUILDJOB_STOCKPILE_SETTINGS))
                    {
                        savekeys = { BUILDJOB_STOCKPILE_SETTINGS };
                    }
                    if (contains(keys, BUILDJOB_STOCKPILE_GIVE_TO))
                    {
                        savekeys = { BUILDJOB_STOCKPILE_GIVE_TO };
                        restore_cursor = true;
                    }
                    if (contains(keys, BUILDJOB_STOCKPILE_MASTER))
                    {
                        savekeys = { BUILDJOB_STOCKPILE_MASTER };
                        restore_cursor = true;
                    }
                }
            }
            break;
        default:
            break;
        }
        if (is_designation_mode(df::global::ui->main.mode))
        {
            // possibly set the new default designation
            for (const auto& _key : keys)
            {
                if (_key >= DESIGNATE_DIG && _key < DESIGNATE_TRAFFIC)
                {
                    ui.m_designate_mode = _key;
                    savekeys = { _key };
                }
                if (_key >= DESIGNATE_STAIR_UP && _key <= DESIGNATE_TOGGLE_ENGRAVING)
                {
                    ui.m_designate_mode = _key;
                    savekeys = { _key };
                }
                if (_key == DESIGNATE_REMOVE_CONSTRUCTION || _key == DESIGNATE_UNDO)
                {
                    ui.m_designate_mode = _key;
                    savekeys = { _key };
                }
                // apply this but don't cache.
                if (_key == DESIGNATE_BITEM)
                {
                    _catch = true;
                    // stability from df's cache
                    apply_key(cl, DESIGNATE_BITEM);
                    keys.clear();
                    savekeys = { DESIGNATE_CLAIM };
                }
                if (_key == DESIGNATE_TRAFFIC)
                {
                    _catch = true;
                    apply_key(cl, DESIGNATE_TRAFFIC);
                    // stability from df's cache
                    keys.clear();
                    savekeys = { DESIGNATE_TRAFFIC_NORMAL };
                }
                // do NOT save scrolling for priority.
                if (_key >= SECONDSCROLL_UP && _key <= SECONDSCROLL_PAGEDOWN)
                {
                    vs->feed_key(_key);
                    savekeys.erase(_key);
                    keys.erase(_key);
                }
            }
        }
        if (is_designation_mode_sub(df::global::ui->main.mode))
        // this is not a designation mode, but a designation mode subscreen
        {
            for (const auto& _key : inkeys) 
            { 
                if (_key >= DESIGNATE_TRAFFIC_LOW && _key <= DESIGNATE_TRAFFIC_DECREASE_WEIGHT_MORE) 
                { 
                    savekeys.insert(_key); 
                } 
                if (_key >= DESIGNATE_CLAIM && _key <= DESIGNATE_DIG_REMOVE_STAIRS_RAMPS)
                {
                    savekeys.insert(_key);
                }
                // do NOT save scrolling for priority.
                if (_key >= SECONDSCROLL_UP && _key <= SECONDSCROLL_PAGEDOWN)
                {
                    savekeys.erase(_key);
                    keys.erase(_key);
                }
            }
        }
    }
    else if (
        id == &df::viewscreen_unitlistst::_identity
    )
    {
        ZOOM_UNIT_ON(UNITJOB_ZOOM_CRE);
    }
    else if (
        id == &df::viewscreen_layer_unit_relationshipst::_identity
    )
    {
        ZOOM_UNIT_ON(UNITVIEW_RELATIONSHIPS_ZOOM);
    }
    else if (
        id == &df::viewscreen_civlistst::_identity
    ) {
        STORE_CURSORSCROLL();
        
        df::viewscreen_civlistst* vs_civ = static_cast<df::viewscreen_civlistst*>(vs);
        for (const auto& _key : inkeys)
        {
            // apply but don't store the mission launch.
            // then switch to missions screen.
            if (_key == CIV_RESCUE || _key == CIV_RECOVER || _key == CIV_RAID)
            {
                vs->feed_key(_key);
                keys.erase(_key);
                savekeys.insert(CIV_MISSIONS);
            }
        }
    }
    else if (
        id == &df::viewscreen_announcelistst::_identity
        #if 0
        || id == &df::viewscreen_reportlistst::_identity
        #endif
    )
    {
        UNSTORE_STANDARDSCROLL();
    }
    else if (id == &df::viewscreen_layer_militaryst::_identity)
    {
        df::viewscreen_layer_militaryst* vs_military = static_cast<df::viewscreen_layer_militaryst*>(vs);
        if (vs_military->equip.in_name_uniform || renaming_squad_id() >= 0)
        {
            UNSTORE_TEXTENTRY();
        }
        else if (vs_military->page == df::viewscreen_layer_militaryst::Alerts)
        {
            if (vs_military->in_rename_alert)
            {
                UNSTORE_TEXTENTRY();
            }
            else
            {
                if (contains(keys, D_MILITARY_ALERTS_DELETE))
                {
                    // we need to apply the erase command and confirm to compensate
                    // for not being able to correctly identify when the confirm dialogue is open.
                    vs->feed_key(D_MILITARY_ALERTS_DELETE);
                    vs->feed_key(MENU_CONFIRM);
                    keys.erase(D_MILITARY_ALERTS_DELETE);
                }
            }
        }
        
        UNSTORE_SECONDSCROLL();
    }
    else if (id == &df::viewscreen_layer_squad_schedulest::_identity)
    {
        df::viewscreen_layer_squad_schedulest* vs_schedule = static_cast<df::viewscreen_layer_squad_schedulest*>(vs);
        assert(vs_schedule);
        if (vs_schedule->in_give_order || vs_schedule->in_edit_order)
        {
            if (contains(keys, SELECT))
            {
                savekeys.insert(SELECT);
            }
            if (contains(keys, D_SQUAD_SCH_GIVE_ORDER))
            {
                savekeys.insert(D_SQUAD_SCH_GIVE_ORDER);
            }
        }
        else if (vs_schedule->in_name_cell)
        {
            // text gets saved by the game.
            UNSTORE_TEXTENTRY();
        }
        else
        {
            if (contains(keys, D_SQUAD_SCH_COPY_ORDERS))
            {
                savekeys.insert(D_SQUAD_SCH_COPY_ORDERS);
            }
        }
    }
}

// reduces the size of keyqueue to last catch, or all the way if there is no catch.
void rewind_keyqueue_to_catch(Client* client)
{
    UIState& ui = client->ui;
    for (auto iter = ui.m_restore_keys.rbegin(); iter != ui.m_restore_keys.rend(); ++iter)
    {
        const RestoreKey& rkey = *iter;
        if (rkey.m_catch)
        {
            // clear key queue past the m_check_start point.
            ui.m_restore_keys.erase(ui.m_restore_keys.begin() + rkey.m_check_start, ui.m_restore_keys.end());
            return;
        }
    }
    
    // no catch found -- so we totally obliterate the keyqueue.
    ui.m_restore_keys.clear();
}

// returns true if successfully shrunk.
// reduces size of keyqueue to after the last time this is matched, ONLY IF there is a match.
static bool try_shrink_keyqueue_to(Client* client, size_t menu_depth, menu_id menu_id)
{
    UIState& ui = client->ui;
    for (auto iter = ui.m_restore_keys.rbegin(); iter != ui.m_restore_keys.rend(); ++iter)
    {
        const RestoreKey& rkey = *iter;
        // check for rkeys which have check-expected-menu.
        if (menu_id == rkey.m_post_menu && rkey.m_post_menu_depth >= menu_depth)
        {
            // we've returned to an earlier state exactly; let's pop commands.
            ui.m_restore_keys.erase(iter.base(), ui.m_restore_keys.end());
            
            return true;
        }
        // check for rkeys which have matter-of-fact observation menu recorded.
        if (rkey.m_catch_observed_autorewind)
        {
            if (menu_id_matches(menu_id, rkey.m_observed_menu) && rkey.m_observed_menu_depth >= menu_depth)
            {
                // we've returned to an earlier state exactly; let's pop commands.
                ui.m_restore_keys.erase(iter.base(), ui.m_restore_keys.end());
                
                return true;
            }
        }
    }
    
    return false;
}

void apply_command(std::set<df::interface_key>& keys, Client* cl, bool raw)
{
    df::viewscreen* vs;
    virtual_identity* id;
    (void)id;
    UPDATE_VS(vs, id);
    
    assert(cl);
    UIState& ui = cl->ui;
    
    const bool at_root = is_at_root();
    
    menu_id menu_id_prev = get_current_menu_id();
    size_t menu_depth_prev = get_vs_depth(vs);
    menu_id menu_id;
    size_t menu_depth;
    
    if (raw)
    {
        // just apply it, don't process it at all.
        vs->feed(&keys);
        UPDATE_VS(vs, id);
        
        // result
        menu_id = get_current_menu_id();
        menu_depth = get_vs_depth(vs);
        keys.clear();
    }
    else
    {
        // check for special cases
        // this may modify the keys list.
        RestoreKey rkey;
        rkey.m_check_start = ui.m_restore_keys.size();
        
        apply_special_case(cl, keys, rkey);
        
        UPDATE_VS(vs, id); // paranoia
        
        // we will now apply the union of keys U rkey.m_interface_keys...
        keys.insert(rkey.m_interface_keys.begin(), rkey.m_interface_keys.end());
        
        
        if (keys.empty())
        {
            return;
        }
        
        // special pause behaviour (configurable in data/init/dfplex.txt)
        if (PAUSE_BEHAVIOUR == PauseBehaviour::EXPLICIT_ANYMENU || PAUSE_BEHAVIOUR == PauseBehaviour::EXPLICIT_DWARFMENU)
        {
            if (contains(keys, D_PAUSE))
            {
                if (is_dwarf_mode() && !vs->key_conflict(D_PAUSE) && !is_realtime_dwarf_menu())
                {
                    const bool toggle_pause =
                           (PAUSE_BEHAVIOUR == PauseBehaviour::EXPLICIT_ANYMENU)
                        || (PAUSE_BEHAVIOUR == PauseBehaviour::EXPLICIT_DWARFMENU && id == &df::viewscreen_dwarfmodest::_identity);
                    
                    global_pause ^= toggle_pause;
                    World::SetPauseState(global_pause);
                }
            }
        }
        
        // apply, but only record it into the restore_keys stack if the result
        // is meaningful.
        {
            // need to copy the keys, since vs->feed can edit it.
            std::set<df::interface_key> _keys = keys;
            
            vs->feed(&_keys);
            UPDATE_VS(vs, id);
        }
        
        // if we have returned to the root menu, clear the record state.
        if (is_at_root())
        {
            ui.m_restore_keys.clear();
            keys.clear();
            return;
        }
        
        menu_id = get_current_menu_id();
        menu_depth = get_vs_depth(vs);
        
        // fill in details if post-menu not set.
        if (rkey.m_post_menu == ::menu_id())
        {
            rkey.m_post_menu = menu_id;
            rkey.m_post_menu_depth = menu_depth;
        }
        
        // one reason we would add the key to the record is if
        // we have gone down a menu.
        if (menu_id != menu_id_prev || menu_depth != menu_depth_prev)
        {
            // menu id has changed
            if (menu_depth <= menu_depth_prev)
            {
                // check to see if it matches an earlier menu.
                if (try_shrink_keyqueue_to(cl, menu_depth, menu_id))
                {
                    // no need to add the key to the record after this;
                    // we're done here.
                    keys.clear();
                    return;
                }
            }
            
            // save all the keys.
            rkey.m_interface_keys.insert(keys.begin(), keys.end());
            rkey.m_check_state = true;
            
            // we've switched menus, so this is a catchpoint.
            rkey.m_catch = true;
        }
        
        // check that we arrived at the expected spot.
        if (!menu_id_matches(rkey.m_post_menu, menu_id))
        {
            // we failed to arrive at the expected menu;
            // cancel out and don't save anything.
            keys.clear();
            return;
        }
        
        // add any savekeys to the keyqueue.
        if (!rkey.m_interface_keys.empty())
        {
            // remove any pointless "NONE" commands
            rkey.m_interface_keys.erase(NONE);
            
            // fill in the cursor position (not always used.)
            Gui::getCursorCoords(rkey.m_cursor.x, rkey.m_cursor.y, rkey.m_cursor.z);
            
            // observed menu
            rkey.m_observed_menu = menu_id;
            rkey.m_observed_menu_depth = menu_depth;
            
            // blockcatch is meant to be used to stop catches afterward.
            if (rkey.m_blockcatch)
            {
                rkey.m_check_start = ui.m_restore_keys.size() + 1;
                rkey.m_check_state = false;
            }
            
            ui.m_restore_keys.push_back(
                std::move(rkey)
            );
        }
        keys.clear();
    }
}
