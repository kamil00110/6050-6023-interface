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

struct SpeakerQuad
{
  size_t topLeft;
  size_t topRight;
  size_t bottomLeft;
  size_t bottomRight;
  
  // Bounding box for this quad
  double minX, maxX;
  double minY, maxY;
  
  bool containsPoint(double x, double y) const
  {
    return x >= minX && x <= maxX && y >= minY && y <= maxY;
  }
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
  std::string playbackId;    // NEW: Unique playback instance ID
  std::string soundId;       // Sound resource ID
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
  // Returns playback instance ID on success, empty string on failure
  std::string playSound(World& world, const std::string& zoneId, double x, double y, 
                        const std::string& soundId, const std::string& playbackId,
                        double volume = 1.0);
  
  // Stop a specific playback instance
  bool stopSound(const std::string& playbackId);
  
  // Stop all instances of a sound resource
  void stopAllInstancesOfSound(const std::string& soundId);
  
  void stopAllSounds();
  
  // Update position of an active sound
  bool updateSoundPosition(World& world, const std::string& playbackId, 
                           double newX, double newY);
  
  // Update volume of an active sound
  bool updateSoundVolume(const std::string& playbackId, double newVolume);
  
  // Query active sounds
  std::vector<std::string> getActivePlaybackIds() const;
  
  bool isSoundPlaying(const std::string& playbackId) const;
  
  // Get information about an active playback
  const ActiveSound* getActiveSoundInfo(const std::string& playbackId) const;
  
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
  
  // Active sounds map - keyed by playback instance ID
  std::map<std::string, ActiveSound> m_activeSounds;
};
