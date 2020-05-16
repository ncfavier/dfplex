# Code overview

DFPlex adds multiplayer into Dwarf mode by using the following simple algorithm every frame *for each player* (see `dfplex.cpp:dfplex_update()`):

1. (`state.cpp:restore_state()`) Restore the player's UI state (where they are looking, their cursor position, current menu, etc.) primarily through two different means:
  - Applying a sequence of key commands that were previously entered by this client. For example, the command sequence `D_NOBLES`, `STANDARDSCROLL_DOWN`, `STANDARDSCROLL_DOWN` will open up the `[n]obles` menu and scroll down twice. (See `state.cpp:apply_restore_key()`)
  - Directly editing dwarf fortress memory -- the cursor position and view position are restored this way, and a few other things as well. This editing occurs before, after, or even during the application of the keypress sequence above. (See `state.cpp`'s `restore_cursor()`, `restore_data()`, `restore_post_state()`, etc.)
2. (`command.cpp:apply_command()`) Apply any new key commands the player has entered this frame. If the command changes the current menu (see `hackutil.cpp:get_current_menu_id()`), it is stored and added to the key command sequence.
  - If we return to a previous menu, instead of adding the key to the sequence, we remove some keys.
  - Certain keys in certain menus are special cased, as the general-purpose logic doesn't work for those keys. See `command.cpp:apply_special_case()`. This is quite a meaty function!
  - Scrolling and typing keys are special cased so that they are always saved (except in certain special cases where they are not).
3. `screenbuf.cpp:perform_render()` Render and store the contents of the screen.
4. `screenbuf.cpp:transfer_screenbuf_client()` Copy the new screen to the client. (To save bandwidth, only the CURSES character information is sent, rather than pixels, and delta-encoding is used.)
5. `hackutil.cpp:return_to_root()` Return to the main dwarfmode screen.
6. (`server.cpp:tock()`) Later, when the client requests an update, the screen information is sent.

Finally, after all player updates have occurred, we set the pause state and return from the plugin `dfplex_update()` so that DF may advance a frame on its own.