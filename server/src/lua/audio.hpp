/**
 * server/src/lua/audio.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_LUA_AUDIO_HPP
#define TRAINTASTIC_SERVER_LUA_AUDIO_HPP

#include <lua.hpp>

namespace Lua::Audio {

/**
 * @brief Push audio table to Lua stack with all audio functions
 */
void push(lua_State* L);

// Global audio functions
int play_sound_at_position(lua_State* L);
int update_sound_position(lua_State* L);
int update_sound_volume(lua_State* L);
int stop_sound(lua_State* L);
int stop_all_sounds(lua_State* L);
int is_sound_playing(lua_State* L);
int get_active_sounds(lua_State* L);
int get_sound_info(lua_State* L);
int get_sound_duration(lua_State* L);
int test_sound_at_position(lua_State* L);

// System volume functions
int get_system_volume(lua_State* L);
int set_system_volume(lua_State* L);
int get_system_mute(lua_State* L);
int set_system_mute(lua_State* L);
int get_device_volume(lua_State* L);
int set_device_volume(lua_State* L);

}

#endif
