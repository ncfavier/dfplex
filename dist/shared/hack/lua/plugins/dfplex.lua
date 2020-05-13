local _ENV = mkmodule('plugins.dfplex')

--[[

 Native functions:

 * get_client_count()
 * get_client_id_by_index(index)
 * get_client_nick(client-id)
 * get_current_menu_id()   
 * get_client_cursorcoord(client-id) -> x, y, z
 * set_client_cursorcoord(client-id, x, y, z)
 * get_client_viewcoord(client-id) -> x, y, z
 * set_client_viewcoord(client-id, x, y, z)
 
 * register_cb_post_state_restore(callback)
   * callback(client-id) -> nil
 
 * lock_dfplex_mutex()
 * unlock_dfplex_mutex()

--]]

return _ENV