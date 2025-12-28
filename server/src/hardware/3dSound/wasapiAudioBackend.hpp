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
  
  // Load audio file into memory (uses soundId as resource identifier)
  bool loadAudioFile(const std::string& filePath, const std::string& soundId);
  void unloadAudioFile(const std::string& soundId);
  
  // Playback control
  // soundId: identifies the audio resource
  // playbackId: unique identifier for this playback instance
  bool playSound(const std::string& soundId,
                 const std::string& playbackId,
                 const std::vector<AudioStreamConfig>& outputs,
                 bool looping,
                 double speed);
  
  // Update active playback streams (for moving sounds)
  bool updateSoundStreams(const std::string& playbackId,
                          const std::vector<AudioStreamConfig>& outputs);
  
  bool stopSound(const std::string& playbackId);
  void stopAllSounds();
  
  bool isSoundPlaying(const std::string& playbackId) const;
  
private:
  WASAPIAudioBackend() = default;
  ~WASAPIAudioBackend();
  
  WASAPIAudioBackend(const WASAPIAudioBackend&) = delete;
  WASAPIAudioBackend& operator=(const WASAPIAudioBackend&) = delete;
  
#ifdef _WIN32
  struct Impl;
  std::unique_ptr<Impl> m_impl;
#endif
  
  // Loaded audio files - keyed by soundId (resource)
  std::map<std::string, AudioFileData> m_audioFiles;
  
  // Active playback sessions - keyed by playbackId (instance)
  std::map<std::string, bool> m_activeSounds;
};
