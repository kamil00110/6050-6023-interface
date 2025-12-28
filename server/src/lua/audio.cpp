/**
 * server/src/lua/audio.cpp
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

#include "audio.hpp"
#include "check.hpp"
#include "checkarguments.hpp"
#include "push.hpp"
#include "to.hpp"
#include "sandbox.hpp"
#include "script.hpp"
#include "../hardware/3dSound/3dAudioPlayer.hpp"
#include "../hardware/3dSound/systemVolumeControl.hpp"
#include "../hardware/3dSound/3dSound.hpp"
#include "../hardware/3dSound/list/3dSoundList.hpp"
#include "../hardware/3dZone/3dZone.hpp"
#include "../world/world.hpp"
#include "../world/getworld.hpp"

namespace Lua::Audio {

void push(lua_State* L)
{
  lua_createtable(L, 0, 16);
  
  // Sound playback functions
  lua_pushcfunction(L, play_sound_at_position);
  lua_setfield(L, -2, "play_sound_at_position");
  
  lua_pushcfunction(L, update_sound_position);
  lua_setfield(L, -2, "update_sound_position");
  
  lua_pushcfunction(L, update_sound_volume);
  lua_setfield(L, -2, "update_sound_volume");
  
  lua_pushcfunction(L, stop_sound);
  lua_setfield(L, -2, "stop_sound");
  
  lua_pushcfunction(L, stop_all_sounds);
  lua_setfield(L, -2, "stop_all_sounds");
  
  lua_pushcfunction(L, is_sound_playing);
  lua_setfield(L, -2, "is_sound_playing");
  
  lua_pushcfunction(L, get_active_sounds);
  lua_setfield(L, -2, "get_active_sounds");
  
  lua_pushcfunction(L, get_sound_info);
  lua_setfield(L, -2, "get_sound_info");
  
  lua_pushcfunction(L, get_sound_duration);
  lua_setfield(L, -2, "get_sound_duration");
  
  lua_pushcfunction(L, test_sound_at_position);
  lua_setfield(L, -2, "test_sound_at_position");
  
  // System volume functions
  lua_pushcfunction(L, get_system_volume);
  lua_setfield(L, -2, "get_system_volume");
  
  lua_pushcfunction(L, set_system_volume);
  lua_setfield(L, -2, "set_system_volume");
  
  lua_pushcfunction(L, get_system_mute);
  lua_setfield(L, -2, "get_system_mute");
  
  lua_pushcfunction(L, set_system_mute);
  lua_setfield(L, -2, "set_system_mute");
  
  lua_pushcfunction(L, get_device_volume);
  lua_setfield(L, -2, "get_device_volume");
  
  lua_pushcfunction(L, set_device_volume);
  lua_setfield(L, -2, "set_device_volume");
}

int play_sound_at_position(lua_State* L)
{
  // Arguments: zone_id, x, y, sound_file_id, [playback_id], [volume]
  const int argc = checkArguments(L, 4, 6);
  
  const std::string zoneId = check<std::string>(L, 1);
  const double x = check<double>(L, 2);
  const double y = check<double>(L, 3);
  const std::string soundId = check<std::string>(L, 4);
  
  // Generate playback ID if not provided
  std::string playbackId;
  if(argc >= 5 && !lua_isnil(L, 5))
  {
    playbackId = check<std::string>(L, 5);
  }
  else
  {
    // Auto-generate: "zone_{zoneId}_sound_{soundId}"
    playbackId = "zone_" + zoneId + "_sound_" + soundId;
  }
  
  const double volume = (argc >= 6) ? check<double>(L, 6) : 1.0;
  
  try
  {
    auto& stateData = Sandbox::getStateData(L);
    auto& world = stateData.script().world();
    
    auto& audioPlayer = ThreeDimensionalAudioPlayer::instance();
    
    // Check if already playing - if so, update position instead
    if(audioPlayer.isSoundPlaying(playbackId))
    {
      if(audioPlayer.updateSoundPosition(world, playbackId, x, y))
      {
        Lua::push(L, playbackId);
        return 1;
      }
    }
    
    // Start new playback
    std::string result = audioPlayer.playSound(
      world,
      zoneId,
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
    
    lua_pushnil(L);
    return 1;
  }
  catch(const std::exception& e)
  {
    errorException(L, e);
  }
}

int update_sound_position(lua_State* L)
{
  // Arguments: zone_id, playback_id, x, y
  checkArguments(L, 4);
  
  const std::string zoneId = check<std::string>(L, 1);
  const std::string playbackId = check<std::string>(L, 2);
  const double x = check<double>(L, 3);
  const double y = check<double>(L, 4);
  
  try
  {
    auto& stateData = Sandbox::getStateData(L);
    auto& world = stateData.script().world();
    
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

int update_sound_volume(lua_State* L)
{
  checkArguments(L, 2);
  
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

int stop_sound(lua_State* L)
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

int stop_all_sounds(lua_State* L)
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

int is_sound_playing(lua_State* L)
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

int get_active_sounds(lua_State* L)
{
  checkArguments(L, 0);
  
  try
  {
    auto playbackIds = ThreeDimensionalAudioPlayer::instance().getActivePlaybackIds();
    
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

int get_sound_info(lua_State* L)
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
    
    lua_createtable(L, 0, 8);
    
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

int get_sound_duration(lua_State* L)
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

int test_sound_at_position(lua_State* L)
{
  // Arguments: zone_id, x, y
  checkArguments(L, 3);
  
  const std::string zoneId = check<std::string>(L, 1);
  const double x = check<double>(L, 2);
  const double y = check<double>(L, 3);
  
  try
  {
    auto& stateData = Sandbox::getStateData(L);
    auto& world = stateData.script().world();
    
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
    
    std::string playbackId = "zone_test_" + zoneId;
    auto& audioPlayer = ThreeDimensionalAudioPlayer::instance();
    
    if(audioPlayer.isSoundPlaying(playbackId))
    {
      audioPlayer.updateSoundPosition(world, playbackId, x, y);
    }
    else
    {
      audioPlayer.playSound(
        world,
        zoneId,
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

// System volume functions

int get_system_volume(lua_State* L)
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

int set_system_volume(lua_State* L)
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

int get_system_mute(lua_State* L)
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

int set_system_mute(lua_State* L)
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

int get_device_volume(lua_State* L)
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

int set_device_volume(lua_State* L)
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
