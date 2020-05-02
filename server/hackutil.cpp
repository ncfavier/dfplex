/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
*/
 
#include "hackutil.hpp"
#include "dfplex.hpp"

#include "modules/Gui.h"
#include "modules/Screen.h"
#include "df/building_trapst.h"
#include "df/global_objects.h"
#include "df/graphic.h"
#include "df/historical_figure.h"
#include "df/invasion_info.h"
#include "df/squad.h"
#include "df/ui_sidebar_menus.h"
#include "df/ui_sidebar_mode.h"
#include "df/ui.h"
#include "df/unit.h"
#include "df/item.h"
#include "df/viewscreen_choose_start_sitest.h"
#include "df/viewscreen_civlistst.h"
#include "df/viewscreen_createquotast.h"
#include "df/viewscreen_customize_unitst.h"
#include "df/viewscreen_dungeonmodest.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/viewscreen_itemst.h"
#include "df/viewscreen_layer_export_play_mapst.h"
#include "df/viewscreen_layer_militaryst.h"
#include "df/viewscreen_layer_squad_schedulest.h"
#include "df/viewscreen_loadgamest.h"
#include "df/viewscreen_meetingst.h"
#include "df/viewscreen_movieplayerst.h"
#include "df/viewscreen_new_regionst.h"
#include "df/viewscreen_optionst.h"
#include "df/viewscreen_overallstatusst.h"
#include "df/viewscreen_setupadventurest.h"
#include "df/viewscreen_textviewerst.h"
#include "df/viewscreen_titlest.h"
#include "df/viewscreen_unitst.h"
#include "df/viewscreen.h"
#include "df/world.h"

using namespace df;
using namespace df::global;
using namespace DFHack;

#define IS_SCREEN(_sc) (id == &df::_sc::_identity)

void show_announcement(std::string announcement)
{
    DFHack::Gui::showAnnouncement(announcement);
}

bool is_at_root()
{
    df::viewscreen* vs;
    df::virtual_identity* id;
    UPDATE_VS(vs, id);
    return (id == &df::viewscreen_dwarfmodest::_identity)
        && df::global::ui->main.mode == df::ui_sidebar_mode::Default;
}

bool is_realtime_dwarf_menu()
{
    df::viewscreen* vs;
    df::virtual_identity* id;
    UPDATE_VS(vs, id);
    return (id == &df::viewscreen_dwarfmodest::_identity)
        && (df::global::ui->main.mode == df::ui_sidebar_mode::Default
            ||df::global::ui->main.mode == df::ui_sidebar_mode::Squads);
}

// force: exit even if that would be a state-changing action.
static void apply_return_for(df::viewscreen* vs, bool force)
{
    virtual_identity* id = df::virtual_identity::get(vs);
    
    // some screens cannot be escaped from without forcing.
    if (!force)
    {
        if (id == &df::viewscreen_meetingst::_identity)
        {
            return;
        }
    }
    
    // some screens require a bit of work to escape from.
    if (id == &df::viewscreen_layer_squad_schedulest::_identity)
    {
        df::viewscreen_layer_squad_schedulest* vs_schedule = static_cast<df::viewscreen_layer_squad_schedulest*>(vs);
        if (vs_schedule->in_name_cell)
        {
            vs->feed_key(df::interface_key::SELECT);
        }
    }
    else if (id == &df::viewscreen_layer_militaryst::_identity)
    {
        df::viewscreen_layer_militaryst* vs_military = static_cast<df::viewscreen_layer_militaryst*>(vs);
        if (vs_military->page == df::viewscreen_layer_militaryst::Alerts)
        {
            if (vs_military->in_rename_alert)
            {
                vs->feed_key(df::interface_key::SELECT);
            }
        }
        else if (vs_military->page == df::viewscreen_layer_militaryst::Uniforms)
        {
            if (vs_military->equip.in_name_uniform)
            {
                vs->feed_key(df::interface_key::SELECT);
            }
        }
        else if (renaming_squad_id() >= 0)
        {
            vs->feed_key(df::interface_key::SELECT);
        }
    }
    vs->feed_key(df::interface_key::LEAVESCREEN);
}
 
// returns false on error.
bool return_to_root()
{
    df::viewscreen* vs;
    virtual_identity* id;
    UPDATE_VS(vs, id);
    if (is_at_root()) return true;
    
    // can't leave the meeting-topic screen.
    // (it would crash the game -- why..?)
    if (id == &df::viewscreen_meetingst::_identity)
    {
        return false;
    }
    
    Gui::resetDwarfmodeView();
    for (size_t i = 0; i < 300; ++i)
    {
        if (is_at_root()) break;
        //UPDATE_VS(vs, id);
        //vs->feed_key(df::interface_key::LEAVESCREEN_ALL); // a segfault once occurred on this line.
        UPDATE_VS(vs, id);
        apply_return_for(vs, true);
        if (Screen::isDismissed(vs))
        {
            // let's just force-kill the screen.
            remove_screen(vs);
        }
    }
    
    return is_at_root();
}

bool defer_return_to_root()
{
    if (is_at_root()) return true;
    
    for (
        df::viewscreen* vs = DFHack::Gui::getCurViewscreen(true);
        vs && vs->parent && vs->parent->parent; // stop at dwarfmode screen
        vs = vs->parent
    )
    {
        apply_return_for(vs, false);
        if (!Screen::isDismissed(vs))
        {
            // failed to leave this screen.
            return false;
        }
    }
    
    Gui::resetDwarfmodeView();
    
    return true;
}

size_t get_vs_depth(df::viewscreen* vs)
{
    size_t i = 0;    
    size_t K_MAX_DEPTH = 1024;
    
    while (vs && i < K_MAX_DEPTH)
    {
        vs = vs->parent;
        if (vs && vs->breakdown_level == df::enums::interface_breakdown_types::NONE)
        {
            ++i;
        }
    }
    
    return i;
}
 
bool is_text_tile(int x, int y, bool &is_map, bool& is_minimap)
{
    df::viewscreen* ws = Gui::getCurViewscreen(true);
    virtual_identity* id = virtual_identity::get(ws);
    assert(ws != NULL);

    int32_t w = gps->dimx, h = gps->dimy;

    is_map = false;
    is_minimap = false;
    
    // screen border
    if (is_dwarf_mode())
    {
        if (!x || !y || x == w - 1 || y == h - 1)
           return true;
    }

    if (IS_SCREEN(viewscreen_dwarfmodest))
    {
        uint8_t menu_width, area_map_width;
        Gui::getMenuWidth(menu_width, area_map_width);
        int32_t menu_left = w - 1, menu_right = w - 1;

        bool menuforced = (df::global::ui->main.mode != df::ui_sidebar_mode::Default || df::global::cursor->x != -30000);

        if ((menuforced || menu_width == 1) && area_map_width == 2) // Menu + area map
        {
            menu_left = w - 56;
            menu_right = w - 25;
        }
        else if (menu_width == 2 && area_map_width == 2) // Area map only
        {
            menu_left = menu_right = w - 25;
        }
        else if (menu_width == 1) // Wide menu
            menu_left = w - 56;
        else if (menuforced || (menu_width == 2 && area_map_width == 3)) // Menu only
            menu_left = w - 32;

        if (x >= menu_left && x <= menu_right)
        {
            if (menuforced && df::global::ui->main.mode == df::ui_sidebar_mode::Burrows && df::global::ui->burrows.in_define_mode)
            {
                // Make burrow symbols use graphics font
                if ((y != 12 && y != 13 && !(x == menu_left + 2 && y == 2)) || x == menu_left || x == menu_right)
                    return true;
            }
            else
                return true;
        }

        is_map = (x > 0 && x < menu_left);
        is_minimap = x > menu_left && x < w - 1;

        return false;
    }
    
    if (IS_SCREEN(viewscreen_civlistst))
    {
        if (x < w - 55)
        {
            is_minimap = true;
            return false;
        }
        
        return true;
    }

    if (IS_SCREEN(viewscreen_dungeonmodest))
    {
        // TODO: Adventure mode

        if (y >= h-2)
            return true;

        return false;
    }

#if 0
    if (IS_SCREEN(viewscreen_setupadventurest))
    {
        df::viewscreen_setupadventurest *s = static_cast<df::viewscreen_setupadventurest*>(ws);
        if (s->subscreen != df::viewscreen_setupadventurest::Nemesis)
            return true;
        else if (x < 58 || x >= 78 || y == 0 || y >= 21)
            return true;

        return false;
    }
  #endif

    if (IS_SCREEN(viewscreen_choose_start_sitest))
    {
        if (y <= 1 || y >= h - 7 || x == 0 || x >= w - 28)
            return true;

        is_minimap = true;
        return false;
    }

    if (IS_SCREEN(viewscreen_new_regionst))
    {
        if (y <= 1 || y >= h - 2 || x <= 37 || x == w - 1)
            return true;

        is_minimap = true;

        return false;
    }

    if (IS_SCREEN(viewscreen_layer_export_play_mapst))
    {
        if (x == w - 1 || x < w - 23)
            return true;

        return false;
    }

    if (IS_SCREEN(viewscreen_overallstatusst))
    {
        if ((x == 46 || x == 71) && y >= 8)
            return false;

        return true;
    }

    if (IS_SCREEN(viewscreen_movieplayerst))
    {
        df::viewscreen_movieplayerst *s = static_cast<df::viewscreen_movieplayerst*>(ws);
        return !s->is_playing;
    }

    /*if (IS_SCREEN(viewscreen_petst))
    {
        if (x == 41 && y >= 7)
            return false;

        return true;
    }*/

    return true;
}

void remove_screen(df::viewscreen* v)
{
    if (v->parent)
    {
        v->parent->child = v->child;
    }
    if (v->child)
    {
        v->child->parent = v->parent;
    }
    
    delete v;
}

bool is_siege()
{
    for (const df::invasion_info* invasion : df::global::ui->invasions.list)
    {
        if (invasion && invasion->flags.bits.siege && invasion->flags.bits.active)
        {
            return true;
        }
    }
    return false;
}

std::string unit_info(int32_t unit_id)
{
    std::stringstream ss;
    ss << "{unit id: " << unit_id;
    const df::unit* unit = df::unit::find(unit_id);
    if (unit)
    {
        ss << ", sex: " << (int32_t)unit->sex;
        const df::language_name& name = unit->name;
        ss << ", name: \"" << name.first_name << "\"";
    }
    ss << "}";
    return ss.str();
}

std::string historical_figure_info(int32_t figure_id)
{
    std::stringstream ss;
    ss << "{historical figure id: " << figure_id;
    const df::historical_figure* figure = df::historical_figure::find(figure_id);
    if (figure)
    {
        ss << ", unit: " << unit_info(figure->unit_id);
    }
    ss << "}";
    return ss.str();
}

std::vector<std::string> word_wrap_lines(const std::string& str, uint16_t width)
{
    std::vector<std::string> out;
    size_t _end = 0;
    while (_end < str.length())
    {
        size_t start = _end;
        size_t last_break = _end;
        while (_end - start < width)
        {
            if (str[_end] == '\n')
            {
                last_break = _end;
                break;
            }
            else if (str[_end] == ' ')
            {
                last_break = _end;
            }
            ++_end;
            if (_end == str.length())
            {
                last_break = _end;
                break;
            }
        }
        if (last_break == start)
        {
            last_break = _end;
        }
        out.emplace_back(str.substr(start, last_break - start));
        _end = last_break;
        if (_end < str.length())
        {
            if (str[_end] == '\n')
            {
                ++_end;
            }
            while (_end < str.length() && str[_end] == ' ')
            {
                ++_end;
            }
        }
    }
    
    return out;
}

// returns a unique string representing the current menu.
menu_id get_current_menu_id()
{
    df::viewscreen* vs;
    virtual_identity* id;
    UPDATE_VS(vs, id);
    
    std::string focus = Gui::getFocusString(vs);
    
    // game adds /Floor, /None, etc; we don't want that.
    if (focus.rfind("dwarfmode/LookAround", 0) == 0)
    {
        focus = "dwarfmode/LookAround";
    }
    
    if (focus.rfind("dwarfmode/ViewUnits", 0) == 0)
    {
        focus = "dwarfmode/ViewUnits";
    }
    
    if (focus.rfind("dwarfmode/BuildingItems", 0) == 0)
    {
        focus = "dwarfmode/BuildingItems";
    }
    
    if (focus.rfind("unitlist", 0) == 0)
    {
        focus = "unitlist";
    }
    
    // remove /On, /Off
    if (startsWith(focus, "layer_stockpile"))
    {
        focus = replace_all(focus, "/On", "");
        focus = replace_all(focus, "/Off", "");
    }
    
    // replace /Job suffix with /Empty
    if (startsWith(focus, "dwarfmode/QueryBuilding"))
    {
        focus = replace_all(focus, "/Job", "/Empty");
    }
    
    const df::enums::ui_sidebar_mode::ui_sidebar_mode LOCATIONS
    = static_cast<df::enums::ui_sidebar_mode::ui_sidebar_mode>((uint32_t)df::enums::ui_sidebar_mode::ArenaTrees + 1);
    
    // append some submenu IDs
    if (id == &df::viewscreen_dwarfmodest::_identity)
    {
        df::viewscreen_dwarfmodest* vs_dwarf = static_cast<df::viewscreen_dwarfmodest*>(vs);
        if (df::global::ui->main.mode == df::enums::ui_sidebar_mode::QueryBuilding)
        {
            df::ui_sidebar_menus& sidebar = *df::global::ui_sidebar_menus;
            if (df::building *selected = df::global::world->selected_building)
            {
                virtual_identity *building_id = virtual_identity::get(selected);
                if (building_id == &df::building_trapst::_identity)
                {
                    df::building_trapst* trap = (df::building_trapst*)selected;
                    if (trap->trap_type == trap_type::Lever)
                    {
                        if (df::global::ui_workshop_in_add && *df::global::ui_workshop_in_add)
                        {
                            // lever target
                            if (df::global::ui_lever_target_type)
                            {
                                focus += "/target";
                                if (*df::global::ui_lever_target_type != df::lever_target_type::NONE)
                                {
                                    focus += "/" + enum_item_key(*df::global::ui_lever_target_type);
                                }
                            }
                        }
                    }
                }
                // workshop subscreen information
                else if (endsWith(focus, "/AddJob"))
                {
                    focus += " query-info: "
                        + std::to_string(sidebar.workshop_job.mat_type) + " "
                        + std::to_string(sidebar.workshop_job.category_id) + " "
                        + std::to_string(sidebar.workshop_job.mat_index) + " "
                        + std::to_string(sidebar.workshop_job.material_category.whole) + " "
                        + std::to_string(sidebar.building.category_id);
                }
            }
        } 
        else if (df::global::ui->main.mode == LOCATIONS)
        {
            df::ui_sidebar_menus& sidebar = *df::global::ui_sidebar_menus;
            if (sidebar.location.in_create)
            {
                focus += "/location/create";
            }
            if (sidebar.location.in_choose_deity)
            {
                focus += "/select-deity";
            }
        }
        else if (df::global::ui->main.mode == df::enums::ui_sidebar_mode::Zones)
        {
            const auto& zone = df::global::ui_sidebar_menus->zone;
            if (zone.remove)
            {
                focus += "/Remove";
            }
            else
            {
                // for some reason, resizing floor zones can cause a segfault??
                //focus += "/" + enum_item_key(zone.mode);
            }
        }
        else if (df::global::ui->main.mode == df::enums::ui_sidebar_mode::Hauling)
        {
            // completely redone this string because the default breaks the requirements too much.
            focus = "dwarfmode/Hauling";
            if (df::global::ui->hauling.in_assign_vehicle)
            {
                focus += "/AssignVehicle";
            }
            else
            {
                if (df::global::ui->hauling.in_name)
                    focus += "/Rename";
                else if (df::global::ui->hauling.in_stop)
                    focus += "/DefineStop";
                else
                    focus += "/Select";
            }
        }
        else if (df::global::ui->main.mode == df::enums::ui_sidebar_mode::ViewUnits)
        {
            if (df::global::ui_selected_unit)
            {
                int32_t selected_unit = *df::global::ui_selected_unit;
                if (df::unit* unit = vector_get(df::global::world->units.active, selected_unit))
                {
                    focus += "/unit-id-" + std::to_string(unit->id);
                }
            }
        }
        else if (df::global::ui->main.mode == df::enums::ui_sidebar_mode::Squads)
        {
            auto& squads = df::global::ui->squads;
            
            // if selecting inside squad, append to menu.
            if (squads.sel_indiv_squad >= 0
                && squads.sel_indiv_squad < static_cast<int32_t>(squads.list.size()))
            {
                df::squad* squad = squads.list.at(squads.sel_indiv_squad);
                if (squad)
                {
                    focus += "/squad-id-" + std::to_string(squad->id);
                }
            }
            if (squads.in_move_order)
            {
                focus += "/move";
            }
            if (squads.in_kill_order)
            {
                focus += "/kill";
            }
            if (squads.in_kill_rect)
            {
                focus += "/rect";
            }
            if (squads.in_kill_list)
            {
                focus += "/list";
            }
        }
    }
    else if (id == &df::viewscreen_createquotast::_identity)
    {
        df::viewscreen_createquotast* vs_quota = static_cast<df::viewscreen_createquotast*>(vs);
        if (vs_quota->want_quantity) focus += "/WantQuantity";
    }
    else if (id == &df::viewscreen_unitst::_identity)
    {
        df::viewscreen_unitst* vs_unit = static_cast<df::viewscreen_unitst*>(vs);
        if (df::unit* unit = vs_unit->unit)
        {
            focus += "/" + std::to_string(unit->id);
        }
    }
    else if (id == &df::viewscreen_customize_unitst::_identity)
    {
        df::viewscreen_customize_unitst* vs_customize_unit = static_cast<df::viewscreen_customize_unitst*>(vs);
        if (vs_customize_unit->editing_nickname)
        {
            focus += "/nickname";
        }
        if (vs_customize_unit->editing_profession)
        {
            focus += "/profession";
        }
    }
    else if (id == &df::viewscreen_layer_squad_schedulest::_identity)
    {
        df::viewscreen_layer_squad_schedulest* vs_schedule = static_cast<df::viewscreen_layer_squad_schedulest*>(vs);
        assert(vs_schedule);
        if (vs_schedule->in_name_cell)
        {
            focus += "/name";
        }
        if (vs_schedule->in_give_order)
        {
            focus += "/order/give";
        }
        if (vs_schedule->in_edit_order)
        {
            focus += "/order/edit";
        }
    }
    else if (id == &df::viewscreen_itemst::_identity)
    {
        df::viewscreen_itemst* vs_item = static_cast<df::viewscreen_itemst*>(vs);
        df::item* item = vs_item->item;
        if (item)
        {
            focus += "/" + std::to_string(item->id);
        }
    }
    else if (id == &df::viewscreen_civlistst::_identity)
    {
        df::viewscreen_civlistst* vs_civ = static_cast<df::viewscreen_civlistst*>(vs);
        focus += "/" + enum_item_key(vs_civ->page);
    }
    else if (id == &df::viewscreen_layer_militaryst::_identity)
    {
        df::viewscreen_layer_militaryst* vs_military = static_cast<df::viewscreen_layer_militaryst*>(vs);
        if (vs_military->page == df::viewscreen_layer_militaryst::Alerts)
        {
            if (vs_military->in_rename_alert)
            {
                focus += "/name";
            }
            if (vs_military->in_delete_alert)
            {
                focus += "/delete";
            }
        }
        else if (vs_military->page == df::viewscreen_layer_militaryst::Uniforms)
        {
            if (vs_military->equip.in_name_uniform)
            {
                focus += "/name";
            }
        }
        else if (vs_military->page == df::viewscreen_layer_militaryst::Ammunition)
        {
            if (vs_military->ammo.in_add_item)
            {
                focus += "/addItem";
            }
            if (vs_military->ammo.in_set_material)
            {
                focus += "/setMaterial";
            }
        }
        else
        {
            if (renaming_squad_id() >= 0)
            {
                focus += "/rename/squad-" + std::to_string(renaming_squad_id());
                *_out << focus << endl;
            }
        }
    }
    
    return focus;
}

int32_t renaming_squad_id()
{
    df::viewscreen* vs;
    virtual_identity* id;
    UPDATE_VS(vs, id);
    if (id == &df::viewscreen_layer_militaryst::_identity)
    {
        df::viewscreen_layer_militaryst* vs_military = static_cast<df::viewscreen_layer_militaryst*>(vs);
        if (vs_military && vs_military->page == df::viewscreen_layer_militaryst::Positions)
        {
            df::squad* rename_squad = df::global::ui_sidebar_menus->unit.rename_squad;
            if (rename_squad)
            {
                return rename_squad->id;
            }
        }
    }
    return -1;
}

void center_view_on_coord(const Coord& _c)
{
    Coord c = _c;
    auto dims = DFHack::Gui::getDwarfmodeViewDims();
    c.x -= (dims.map_x2 - dims.map_x1) / 2;
    c.y -= (dims.y2 - dims.y1) / 2;
    if (c.x < 0) c.x = 0;
    if (c.y < 0) c.y = 0;
    Gui::setViewCoords(c.x, c.y, c.z);
}