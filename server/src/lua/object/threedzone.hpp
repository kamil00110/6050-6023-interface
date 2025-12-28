/**
 * server/src/lua/object/threedzone.hpp
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

#ifndef TRAINTASTIC_SERVER_LUA_OBJECT_THREEDZONE_HPP
#define TRAINTASTIC_SERVER_LUA_OBJECT_THREEDZONE_HPP

#include <lua.hpp>
#include "interface.hpp"

class ThreeDZone;

namespace Lua::Object {

class ThreeDZone
{
public:
  static constexpr char const* metaTableName = "three_d_zone";

  static void registerType(lua_State* L);
  static int index(lua_State* L, ThreeDZone& object);

private:
  static int __index(lua_State* L);
  
  // 3D Audio methods
  static int play_sound_at_position(lua_State* L);
  static int update_sound_position(lua_State* L);
  static int update_sound_volume(lua_State* L);
  static int stop_sound(lua_State* L);
  static int stop_all_sounds(lua_State* L);
  static int is_sound_playing(lua_State* L);
  static int get_active_sounds(lua_State* L);
  static int get_sound_info(lua_State* L);
  static int get_sound_duration(lua_State* L);
  static int test_sound_at_position(lua_State* L);
  
  // System volume control methods
  static int get_system_volume(lua_State* L);
  static int set_system_volume(lua_State* L);
  static int get_system_mute(lua_State* L);
  static int set_system_mute(lua_State* L);
  static int get_device_volume(lua_State* L);
  static int set_device_volume(lua_State* L);
};

}

#endif
