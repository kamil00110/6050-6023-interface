/**
 * server/src/utils/audioenumerator.hpp
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

#pragma once

#include <string>
#include <vector>
#include <optional>

struct AudioChannelInfo
{
  std::string channelName;
  uint32_t channelIndex;
};

struct AudioDeviceInfo
{
  std::string deviceId;
  std::string deviceName;
  uint32_t channelCount;
  std::vector<AudioChannelInfo> channels;
  bool isDefault;
};

class AudioEnumerator
{
public:
  // Enumerate all audio output devices
  static std::vector<AudioDeviceInfo> enumerateDevices();
  
  // Log all device information
  static void logDevices();
  
  // Get speaker name by ID, returns "Sound controller missing" if not found
  static std::string getSpeakerName(const std::string& deviceId);
  
  // Get list of all speaker IDs
  static std::vector<std::string> listSpeakerIds();
  
  // Get channel count for a specific speaker, returns 0 if not found
  static uint32_t getSpeakerChannels(const std::string& deviceId);
  
  // Get detailed channel information for a specific speaker
  static std::vector<AudioChannelInfo> getSpeakerChannelInfo(const std::string& deviceId);
};
