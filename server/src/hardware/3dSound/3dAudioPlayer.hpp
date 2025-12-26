/**
 * server/src/hardware/3dSound/3dAudioPlayer.hpp
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
#include <memory>
#include <vector>
#include <map>

// Forward declarations
class World;
class ThreeDZone;
class ThreeDSound;

struct SpeakerPosition
{
  int id;
  double x;
  double y;
  std::string label;
  std::string deviceId;
  int channel;
  double volume;
};

struct SpeakerOutput
{
  std::string deviceId;
  int channel;
  double volume;
  double delay; // in milliseconds
};

struct ActiveSound
{
  std::string soundId;
  std::string zoneId;
  double x;
  double y;
  double volume;
  bool looping;
  double speed;
  std::vector<SpeakerOutput> speakerOutputs;
  uint64_t startTime; // For tracking playback
};

class ThreeDimensionalAudioPlayer
{
public:
  static ThreeDimensionalAudioPlayer& instance();
  
  // Main playback control
  bool playSound(World& world, const std::string& zoneId, double x, double y, 
                 const std::string& soundId, double volume = 1.0);
  
  bool stopSound(const std::string& soundId);
  
  void stopAllSounds();
  
  // Query active sounds
  std::vector<std::string> getActiveSounds() const;
  
  bool isSoundPlaying(const std::string& soundId) const;
  
private:
  ThreeDimensionalAudioPlayer() = default;
  
  // Calculate speaker outputs for a sound at position
  std::vector<SpeakerOutput> calculateSpeakerOutputs(
    const std::vector<SpeakerPosition>& speakers,
    double soundX, double soundY,
    double zoneWidth, double zoneHeight,
    double masterVolume);
  
  // Parse zone speakers from JSON
  std::vector<SpeakerPosition> parseZoneSpeakers(const std::string& speakersJson);
  
  // Calculate distance between two points
  double calculateDistance(double x1, double y1, double x2, double y2) const;
  
  // Calculate delay based on distance (speed of sound = 343 m/s)
  double calculateDelay(double distance) const;
  
  // Calculate volume attenuation using inverse square law
  double calculateAttenuation(double distance, double maxDistance) const;
  
  // Calculate horizontal panning for multi-speaker setups
  std::vector<double> calculatePanning(
    const std::vector<SpeakerPosition>& speakers,
    double soundX, double soundY,
    double zoneWidth, double zoneHeight) const;
  
  // Active sounds map
  std::map<std::string, ActiveSound> m_activeSounds;
  std::vector<SpeakerQuad> generateQuads(
    const std::vector<SpeakerPosition>& speakers) const;
    
  std::vector<double> calculateQuadPanning(
    const std::vector<SpeakerPosition>& speakers,
    const SpeakerQuad& quad,
    double soundX, double soundY) const;
    
  std::vector<double> calculateSimplePanning(
    const std::vector<SpeakerPosition>& speakers,
    double soundX, double soundY,
    double zoneWidth, double zoneHeight) const;
};
