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
#include "../../hardware/3dSound/systemVolumeControl.hpp"
#include "../../world/getworld.hpp"

namespace Lua::Object {

void ThreeDZone::registerType(lua_State* L)
{
  MetaTable::clone(L, Object::metaTableName, metaTableName);
  lua_pushcfunction(L, __index);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

int ThreeDZone::index(lua_State* L, ::ThreeDZone& zone)
{
  const auto key = to<std::string_view>(L, 2);
  
  // 3D Audio methods
  if(key == "play_sound_at_position")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, play_sound_at_position, 1);
    return 1;
  }
  if(key == "update_sound_position")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, update_sound_position, 1);
    return 1;
  }
  if(key == "update_sound_volume")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, update_sound_volume, 1);
    return 1;
  }
  if(key == "stop_sound")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, stop_sound, 1);
    return 1;
  }
  if(key == "stop_all_sounds")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, stop_all_sounds, 1);
    return 1;
  }
  if(key == "is_sound_playing")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, is_sound_playing, 1);
    return 1;
  }
  if(key == "get_active_sounds")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, get_active_sounds, 1);
    return 1;
  }
  if(key == "get_sound_info")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, get_sound_info, 1);
    return 1;
  }
  if(key == "get_sound_duration")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, get_sound_duration, 1);
    return 1;
  }
  if(key == "test_sound_at_position")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, test_sound_at_position, 1);
    return 1;
  }
  
  // System volume control methods
  if(key == "get_system_volume")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, get_system_volume, 1);
    return 1;
  }
  if(key == "set_system_volume")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, set_system_volume, 1);
    return 1;
  }
  if(key == "get_system_mute")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, get_system_mute, 1);
    return 1;
  }
  if(key == "set_system_mute")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, set_system_mute, 1);
    return 1;
  }
  if(key == "get_device_volume")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, get_device_volume, 1);
    return 1;
  }
  if(key == "set_device_volume")
  {
    lua_pushvalue(L, 1);
    lua_pushcclosure(L, set_device_volume, 1);
    return 1;
  }
  
  // Fall back to Object methods
  return Object::index(L, zone);
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
        Lua::push(L, playbackId);
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
      Lua::push(L, result);
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
    
    Lua::push(L, success);
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
    
    Lua::push(L, success);
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
    Lua::push(L, success);
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
    Lua::push(L, playing);
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
      Lua::push(L, id);
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
    
    Lua::push(L, info->playbackId);
    lua_setfield(L, -2, "playback_id");
    
    Lua::push(L, info->soundId);
    lua_setfield(L, -2, "sound_id");
    
    Lua::push(L, info->zoneId);
    lua_setfield(L, -2, "zone_id");
    
    Lua::push(L, info->x);
    lua_setfield(L, -2, "x");
    
    Lua::push(L, info->y);
    lua_setfield(L, -2, "y");
    
    Lua::push(L, info->volume);
    lua_setfield(L, -2, "volume");
    
    Lua::push(L, info->looping);
    lua_setfield(L, -2, "looping");
    
    Lua::push(L, info->speed);
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
    
    // Call the Method<void(std::string)> - we need to get it and invoke it properly
    // Instead of calling testSoundAtPosition directly, we'll replicate its logic here
    
    auto& world = getWorld(*zone);
    auto soundsList = world.threeDSounds.value();
    
    if(!soundsList || soundsList->empty())
    {
      return 0;
    }
    
    auto firstSound = soundsList->front();
    if(!firstSound)
    {
      return 0;
    }
    
    // Generate playback ID for test sounds
    std::string playbackId = "zone_test_" + zone->id.value();
    
    auto& audioPlayer = ThreeDimensionalAudioPlayer::instance();
    
    // Check if test sound is already playing
    if(audioPlayer.isSoundPlaying(playbackId))
    {
      // Update position
      audioPlayer.updateSoundPosition(world, playbackId, x, y);
    }
    else
    {
      // Start new test sound
      audioPlayer.playSound(
        world,
        zone->id.value(),
        x, y,
        firstSound->id.value(),
        playbackId,
        1.0
      );
    }
    
    return 0;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::get_sound_duration(lua_State* L)
{
  checkArguments(L, 1);
  
  const std::string soundId = check<std::string>(L, 1);
  
  try
  {
    double duration = ThreeDimensionalAudioPlayer::instance().getSoundDuration(soundId);
    Lua::push(L, duration);
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

// System volume control methods

int ThreeDZone::get_system_volume(lua_State* L)
{
  checkArguments(L, 0);
  
  try
  {
    double volume = SystemVolumeControl::instance().getSystemVolume();
    if(volume < 0.0)
    {
      lua_pushnil(L);
    }
    else
    {
      Lua::push(L, volume);
    }
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::set_system_volume(lua_State* L)
{
  checkArguments(L, 1);
  
  const double volume = check<double>(L, 1);
  
  try
  {
    bool success = SystemVolumeControl::instance().setSystemVolume(volume);
    Lua::push(L, success);
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::get_system_mute(lua_State* L)
{
  checkArguments(L, 0);
  
  try
  {
    bool muted = SystemVolumeControl::instance().getSystemMute();
    Lua::push(L, muted);
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::set_system_mute(lua_State* L)
{
  checkArguments(L, 1);
  
  const bool muted = check<bool>(L, 1);
  
  try
  {
    bool success = SystemVolumeControl::instance().setSystemMute(muted);
    Lua::push(L, success);
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::get_device_volume(lua_State* L)
{
  const int argc = checkArguments(L, 0, 1);
  
  const std::string deviceId = (argc >= 1) ? check<std::string>(L, 1) : "";
  
  try
  {
    double volume = SystemVolumeControl::instance().getDeviceVolume(deviceId);
    if(volume < 0.0)
    {
      lua_pushnil(L);
    }
    else
    {
      Lua::push(L, volume);
    }
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int ThreeDZone::set_device_volume(lua_State* L)
{
  checkArguments(L, 2);
  
  const std::string deviceId = check<std::string>(L, 1);
  const double volume = check<double>(L, 2);
  
  try
  {
    bool success = SystemVolumeControl::instance().setDeviceVolume(deviceId, volume);
    Lua::push(L, success);
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

}
