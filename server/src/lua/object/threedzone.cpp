/**
 * server/src/lua/object/threedzone.cpp
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

#include "threedzone.hpp"
#include "object.hpp"
#include "../check.hpp"
#include "../checkarguments.hpp"
#include "../push.hpp"
#include "../to.hpp"
#include "../metatable.hpp"
#include "../../hardware/3dZone/3dZone.hpp"
#include "../../hardware/3dSound/3dAudioPlayer.hpp"
#include "../../world/getworld.hpp"

#define LUA_OBJECT_PROPERTY(name) \
  if(key == #name)

#define LUA_OBJECT_METHOD(name) \
  if(key == #name) \
  { \
    lua_pushvalue(L, 1); \
    lua_pushcclosure(L, name, 1); \
    return 1; \
  }

namespace Lua::Object {

void ThreeDZone::registerType(lua_State* L)
{
  MetaTable::clone(L, Interface::metaTableName, metaTableName);
  lua_pushcfunction(L, __index);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

int ThreeDZone::index(lua_State* L, ::ThreeDZone& zone)
{
  const auto key = to<std::string_view>(L, 2);
  
  // 3D Audio methods
  LUA_OBJECT_METHOD(play_sound_at_position)
  LUA_OBJECT_METHOD(update_sound_position)
  LUA_OBJECT_METHOD(update_sound_volume)
  LUA_OBJECT_METHOD(stop_sound)
  LUA_OBJECT_METHOD(stop_all_sounds)
  LUA_OBJECT_METHOD(is_sound_playing)
  LUA_OBJECT_METHOD(get_active_sounds)
  LUA_OBJECT_METHOD(get_sound_info)
  LUA_OBJECT_METHOD(test_sound_at_position)
  
  // Fall back to Interface methods
  return Interface::index(L, zone);
}

int ThreeDZone::__index(lua_State* L)
{
  return index(L, *check<::ThreeDZone>(L, 1));
}

int ThreeDZone::play_sound_at_position(lua_State* L)
{
  // Arguments: x, y, sound_id, [playback_id], [volume]
  const int argc = checkArguments(L, 3, 5);
  
  auto zone = check<::ThreeDZone>(L, lua_upvalueindex(1));
  auto& world = getWorld(*zone);
  
  const double x = check<double>(L, 1);
  const double y = check<double>(L, 2);
  const std::string soundId = check<std::string>(L, 3);
  
  // Generate playback ID if not provided
  std::string playbackId;
  if(argc >= 4 && !lua_isnil(L, 4))
  {
    playbackId = check<std::string>(L, 4);
  }
  else
  {
    // Auto-generate: "zone_{zoneId}_sound_{soundId}"
    playbackId = "zone_" + zone->id.value() + "_sound_" + soundId;
  }
  
  const double volume = (argc >= 5) ? check<double>(L, 5) : 1.0;
  
  try
  {
    auto& audioPlayer = ThreeDimensionalAudioPlayer::instance();
    
    // Check if already playing - if so, update position instead
    if(audioPlayer.isSoundPlaying(playbackId))
    {
      if(audioPlayer.updateSoundPosition(world, playbackId, x, y))
      {
        push(L, playbackId);
        return 1;
      }
      // If update failed, fall through to start new playback
    }
    
    // Start new playback
    std::string result = audioPlayer.playSound(
      world,
      zone->id.value(),
      x, y,
      soundId,
      playbackId,
      volume
    );
    
    if(!result.empty())
    {
      push(L, result);
      return 1;
    }
    
    // Failed
    lua_pushnil(L);
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::update_sound_position(lua_State* L)
{
  checkArguments(L, 3);
  
  auto zone = check<::ThreeDZone>(L, lua_upvalueindex(1));
  auto& world = getWorld(*zone);
  
  const std::string playbackId = check<std::string>(L, 1);
  const double x = check<double>(L, 2);
  const double y = check<double>(L, 3);
  
  try
  {
    bool success = ThreeDimensionalAudioPlayer::instance().updateSoundPosition(
      world, playbackId, x, y
    );
    
    push(L, success);
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::update_sound_volume(lua_State* L)
{
  checkArguments(L, 2);
  
  auto zone = check<::ThreeDZone>(L, lua_upvalueindex(1));
  
  const std::string playbackId = check<std::string>(L, 1);
  const double volume = check<double>(L, 2);
  
  try
  {
    bool success = ThreeDimensionalAudioPlayer::instance().updateSoundVolume(
      playbackId, volume
    );
    
    push(L, success);
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::stop_sound(lua_State* L)
{
  checkArguments(L, 1);
  
  const std::string playbackId = check<std::string>(L, 1);
  
  try
  {
    bool success = ThreeDimensionalAudioPlayer::instance().stopSound(playbackId);
    push(L, success);
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::stop_all_sounds(lua_State* L)
{
  checkArguments(L, 0);
  
  try
  {
    ThreeDimensionalAudioPlayer::instance().stopAllSounds();
    return 0;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::is_sound_playing(lua_State* L)
{
  checkArguments(L, 1);
  
  const std::string playbackId = check<std::string>(L, 1);
  
  try
  {
    bool playing = ThreeDimensionalAudioPlayer::instance().isSoundPlaying(playbackId);
    push(L, playing);
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::get_active_sounds(lua_State* L)
{
  checkArguments(L, 0);
  
  try
  {
    auto playbackIds = ThreeDimensionalAudioPlayer::instance().getActivePlaybackIds();
    
    // Create Lua table
    lua_createtable(L, static_cast<int>(playbackIds.size()), 0);
    lua_Integer n = 1;
    for(const auto& id : playbackIds)
    {
      push(L, id);
      lua_rawseti(L, -2, n);
      n++;
    }
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::get_sound_info(lua_State* L)
{
  checkArguments(L, 1);
  
  const std::string playbackId = check<std::string>(L, 1);
  
  try
  {
    const auto* info = ThreeDimensionalAudioPlayer::instance().getActiveSoundInfo(playbackId);
    
    if(!info)
    {
      lua_pushnil(L);
      return 1;
    }
    
    // Create info table
    lua_createtable(L, 0, 7);
    
    push(L, info->playbackId);
    lua_setfield(L, -2, "playback_id");
    
    push(L, info->soundId);
    lua_setfield(L, -2, "sound_id");
    
    push(L, info->zoneId);
    lua_setfield(L, -2, "zone_id");
    
    push(L, info->x);
    lua_setfield(L, -2, "x");
    
    push(L, info->y);
    lua_setfield(L, -2, "y");
    
    push(L, info->volume);
    lua_setfield(L, -2, "volume");
    
    push(L, info->looping);
    lua_setfield(L, -2, "looping");
    
    push(L, info->speed);
    lua_setfield(L, -2, "speed");
    
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::test_sound_at_position(lua_State* L)
{
  checkArguments(L, 2);
  
  auto zone = check<::ThreeDZone>(L, lua_upvalueindex(1));
  
  const double x = check<double>(L, 1);
  const double y = check<double>(L, 2);
  
  try
  {
    // Build coordinate string for the existing method
    std::string coords = std::to_string(x) + "," + std::to_string(y);
    
    // Call the existing C++ method
    zone->testSoundAtPosition(coords);
    
    return 0;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

}
