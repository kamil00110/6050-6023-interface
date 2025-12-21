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
#include <memory>

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
  static std::unique_ptr<AudioEnumerator> create();
  
  ~AudioEnumerator();
  
  // Enumerate all audio output devices
  std::vector<AudioDeviceInfo> enumerateDevices();
  
  // Log all device information
  void logDeviceInfo();
  
private:
  AudioEnumerator();
  
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
