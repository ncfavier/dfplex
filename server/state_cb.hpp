#pragma once

/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2020 white-rabbit, ISC license
 */

 #include "Client.hpp"

int suppress_sidebar_refresh(Client*);

// restores cursor from the cursor coordinates at the time of calling this function.
restore_state_cb_t produce_restore_cb_restore_cursor();

// restores cursor coordinates from the previous frame's coordinates
int restore_cursor(Client*);

int restore_squads_state(Client*);

int restore_unit_view_state(Client*);