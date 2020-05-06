local utils = require('utils')
local repeatUtil = require('repeat-util')
local dfplex = require('plugins.dfplex')

print("Enabling restrict-z")

-- https://stackoverflow.com/a/22831842
function string.starts(String,Start)
   return string.sub(String,1,string.len(Start))==Start
end

local restrict_z = function(client)
    local flerb = dfplex.get_current_menu_id()
    
    -- some permitted menus
    if string.starts(flerb,"dwarfmode") ~= true then
        return
    end
    if string.starts(flerb,"dwarfmode/LookAround") then
        return
    end
    if string.starts(flerb,"dwarfmode/viewunit") then
        return
    end
    if string.starts(flerb,"dwarfmode/BuildingItems") then
        return
    end
    
    -- otherwise, force cursor coordinates if cursor is visible.
    local x, y, z = dfplex.get_client_cursorcoord(client)
    if x >= 0 and y >= 0 then
        -- x, y set, so cursor is visible.
        local z_required = 175
        if z ~= z_required then
            -- set cursor and view coordinate.
            dfplex.set_client_cursorcoord(client, x, y, z_required)
            local xv, yv, zv = dfplex.get_client_viewcoord(client)
            dfplex.set_client_viewcoord(client, xv, yv, z_required)
        end
    end
end

dfplex.register_cb_post_state_restore(restrict_z)