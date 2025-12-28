/**
 * server/src/hardware/3dSound/wasapiAudioBackend.hpp
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
#include <map>
#include <cstdint>

struct AudioFileData;

struct AudioStreamConfig
{
  std::string deviceId;
  int channel;
  double volume;
  double delay; // milliseconds
};

class WASAPIAudioBackend
{
public:
  static WASAPIAudioBackend& instance();
  
  // Initialize/cleanup
  bool initialize();
  void shutdown();
  
  // Load audio file into memory
  // Note: soundId is now used as instanceId for supporting multiple instances
  bool loadAudioFile(const std::string& filePath, const std::string& instanceId);
  void unloadAudioFile(const std::string& instanceId);
  
  // Playback control
  // Note: soundId parameter is now instanceId
  bool playSound(const std::string& instanceId, 
                 const std::vector<AudioStreamConfig>& outputs,
                 bool looping,
                 double speed);
  
  bool stopSound(const std::string& instanceId);
  void stopAllSounds();
  
  // Update sound parameters (volume, delay) without stopping playback
  bool updateSound(const std::string& instanceId,
                   const std::vector<AudioStreamConfig>& newOutputs);
  
  bool isSoundPlaying(const std::string& instanceId) const;
  
private:
  WASAPIAudioBackend() = default;
  ~WASAPIAudioBackend();
  
  WASAPIAudioBackend(const WASAPIAudioBackend&) = delete;
  WASAPIAudioBackend& operator=(const WASAPIAudioBackend&) = delete;
  
#ifdef _WIN32
  struct Impl;
  std::unique_ptr<Impl> m_impl;
#endif
  
  // Loaded audio files - keyed by instance ID
  std::map<std::string, AudioFileData> m_audioFiles;
  
  // Active playback sessions - keyed by instance ID
  std::map<std::string, bool> m_activeSounds;
};
