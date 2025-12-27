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
#include <atomic>

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
  std::string instanceId;  // Unique instance identifier (custom or auto-generated)
  std::string soundId;     // Original sound object ID
  std::string zoneId;
  double x;
  double y;
  double volume;
  bool looping;
  double speed;
  std::vector<SpeakerPosition> speakers; // Store speaker configuration
  std::vector<SpeakerOutput> speakerOutputs;
  uint64_t startTime; // For tracking playback
  double zoneWidth;   // Store zone dimensions for moveSound
  double zoneHeight;
};

struct PlayingSoundInfo
{
  std::string instanceId;
  std::string soundId;
  std::string zoneId;
  double x;
  double y;
  double volume;
  bool looping;
  double speed;
  uint64_t startTime;
};

class ThreeDimensionalAudioPlayer
{
public:
  static ThreeDimensionalAudioPlayer& instance();
  
  // Main playback control with custom instance ID
  // Returns true if successful, false if instanceId is already in use
  bool playSound(World& world, const std::string& zoneId, double x, double y, 
                 const std::string& instanceId, const std::string& soundId, 
                 double volume = 1.0);
  
  // Stop specific sound instance
  bool stopSound(const std::string& instanceId);
  
  // Move a playing sound to a new position without stopping it
  // Returns false if sound is not playing or position is invalid
  bool moveSound(const std::string& instanceId, double newX, double newY);
  
  // Update volume of a playing sound
  bool updateSoundVolume(const std::string& instanceId, double newVolume);
  
  // Stop all instances of a specific sound object
  void stopAllInstancesOfSound(const std::string& soundId);
  
  // Stop all sounds
  void stopAllSounds();
  
  // Query active sounds
  std::vector<std::string> getActiveSoundInstances() const;
  
  std::vector<PlayingSoundInfo> getAllPlayingSounds() const;
  
  bool isSoundInstancePlaying(const std::string& instanceId) const;
  
  // Count how many instances of a sound are playing
  int countPlayingInstancesOfSound(const std::string& soundId) const;
  
  // Get detailed info about a specific playing sound
  bool getSoundInfo(const std::string& instanceId, PlayingSoundInfo& info) const;
  
private:
  ThreeDimensionalAudioPlayer() : m_nextInstanceId(1) {}
  
  // Generate unique instance ID (for potential future auto-generation feature)
  std::string generateInstanceId();
  
  // Internal method to recalculate and update speaker outputs for a sound
  bool recalculateSpeakerOutputs(ActiveSound& activeSound);
  
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
  
  // Active sounds map - keyed by instance ID
  std::map<std::string, ActiveSound> m_activeSounds;
  
  // Atomic counter for generating unique instance IDs (for future use)
  std::atomic<uint64_t> m_nextInstanceId;
};
