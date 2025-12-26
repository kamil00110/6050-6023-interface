/**
 * server/src/hardware/3dSound/3dAudioPlayer.cpp
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

#include "3dAudioPlayer.hpp"
#include "3dSound.hpp"
#include "wasapiAudioBackend.hpp"
#include "../3dZone/3dZone.hpp"
#include "../../world/world.hpp"
#include "../../log/log.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>

using nlohmann::json;

constexpr double SPEED_OF_SOUND = 343.0; // meters per second
constexpr double MIN_DISTANCE = 0.01; // minimum distance to avoid division by zero

ThreeDimensionalAudioPlayer& ThreeDimensionalAudioPlayer::instance()
{
  static ThreeDimensionalAudioPlayer instance;
  return instance;
}

bool ThreeDimensionalAudioPlayer::playSound(World& world, const std::string& zoneId, 
                                             double x, double y, 
                                             const std::string& soundId, 
                                             double volume)
{
  try
  {
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Attempting to play sound '") + soundId + "' in zone '" + zoneId + 
      "' at position (" + std::to_string(x) + ", " + std::to_string(y) + ")");
    
    // Get zone object
    auto zoneObj = std::dynamic_pointer_cast<ThreeDZone>(world.getObjectById(zoneId));
    if(!zoneObj)
    {
      Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
        std::string("Zone not found: ") + zoneId);
      return false;
    }
    
    // Get sound object
    auto soundObj = std::dynamic_pointer_cast<ThreeDSound>(world.getObjectById(soundId));
    if(!soundObj)
    {
      Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
        std::string("Sound not found: ") + soundId);
      return false;
    }
    
    // Get zone dimensions
    double zoneWidth = zoneObj->width.value();
    double zoneHeight = zoneObj->height.value();
    
    // Validate position is within zone
    if(x < 0 || x > zoneWidth || y < 0 || y > zoneHeight)
    {
      Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
        std::string("Position out of zone bounds"));
      return false;
    }
    
    // Parse speaker positions from zone
    auto speakers = parseZoneSpeakers(zoneObj->speakersData.value());
    if(speakers.empty())
    {
      Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
        std::string("No speakers configured in zone"));
      return false;
    }
    
    // Calculate speaker outputs
    auto outputs = calculateSpeakerOutputs(speakers, x, y, zoneWidth, zoneHeight, volume);
    
    // Log calculated outputs
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Calculated outputs for ") + std::to_string(outputs.size()) + " speakers:");
    for(const auto& output : outputs)
    {
      Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
        std::string("  Device: ") + output.deviceId + ", Channel: " + 
        std::to_string(output.channel) + ", Volume: " + 
        std::to_string(output.volume) + ", Delay: " + 
        std::to_string(output.delay) + "ms");
    }
    
    // Stop existing sound if already playing
    if(m_activeSounds.count(soundId))
    {
      stopSound(soundId);
    }
    
    // Get audio file path
    const auto audioDir = world.audioFilesDir();
    const auto audioFilePath = audioDir / soundObj->soundFile.value();
    
    // Check if audio file exists
    if(soundObj->soundFile.value().empty() || !std::filesystem::exists(audioFilePath))
    {
      Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
        std::string("Audio file not found or not set for sound: ") + soundId);
      return false;
    }
    
    // Initialize WASAPI backend if needed
    auto& backend = WASAPIAudioBackend::instance();
    static bool backendInitialized = false;
    if(!backendInitialized)
    {
      backendInitialized = backend.initialize();
      if(!backendInitialized)
      {
        Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
          std::string("Failed to initialize audio backend"));
        return false;
      }
    }
    
    // Load audio file
    if(!backend.loadAudioFile(audioFilePath.string(), soundId))
    {
      Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
        std::string("Failed to load audio file: ") + audioFilePath.string());
      return false;
    }
    
    // Convert speaker outputs to audio stream configs
    std::vector<AudioStreamConfig> streamConfigs;
    for(const auto& output : outputs)
    {
      AudioStreamConfig config;
      config.deviceId = output.deviceId;
      config.channel = output.channel;
      config.volume = output.volume;
      config.delay = output.delay;
      streamConfigs.push_back(config);
    }
    
    // Start playback through WASAPI
    bool looping = soundObj->looping.value();
    double speed = soundObj->speed.value();
    
    if(!backend.playSound(soundId, streamConfigs, looping, speed))
    {
      Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
        std::string("Failed to start playback"));
      backend.unloadAudioFile(soundId);
      return false;
    }
    
    // Create active sound entry
    ActiveSound activeSound;
    activeSound.soundId = soundId;
    activeSound.zoneId = zoneId;
    activeSound.x = x;
    activeSound.y = y;
    activeSound.volume = volume;
    activeSound.looping = looping;
    activeSound.speed = speed;
    activeSound.speakerOutputs = outputs;
    activeSound.startTime = 0; // TODO: Set to current time in milliseconds
    
    m_activeSounds[soundId] = activeSound;
    
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Sound '") + soundId + "' started " + 
      (activeSound.looping ? "(looping)" : "(one-shot)"));
    
    return true;
  }
  catch(const std::exception& e)
  {
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Error playing sound: ") + e.what());
    return false;
  }
}

bool ThreeDimensionalAudioPlayer::stopSound(const std::string& soundId)
{
  auto it = m_activeSounds.find(soundId);
  if(it == m_activeSounds.end())
  {
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Sound not playing: ") + soundId);
    return false;
  }
  
  Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
    std::string("Stopping sound: ") + soundId);
  
  // Stop playback through WASAPI backend
  WASAPIAudioBackend::instance().stopSound(soundId);
  
  // Unload audio file
  WASAPIAudioBackend::instance().unloadAudioFile(soundId);
  
  m_activeSounds.erase(it);
  return true;
}

void ThreeDimensionalAudioPlayer::stopAllSounds()
{
  Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
    std::string("Stopping all sounds (") + std::to_string(m_activeSounds.size()) + " active)");
  
  // Stop all sounds through WASAPI backend
  WASAPIAudioBackend::instance().stopAllSounds();
  
  // Unload all audio files
  for(const auto& [soundId, _] : m_activeSounds)
  {
    WASAPIAudioBackend::instance().unloadAudioFile(soundId);
  }
  
  m_activeSounds.clear();
}

std::vector<std::string> ThreeDimensionalAudioPlayer::getActiveSounds() const
{
  std::vector<std::string> sounds;
  for(const auto& [soundId, _] : m_activeSounds)
  {
    sounds.push_back(soundId);
  }
  return sounds;
}

bool ThreeDimensionalAudioPlayer::isSoundPlaying(const std::string& soundId) const
{
  return m_activeSounds.count(soundId) > 0;
}

std::vector<SpeakerOutput> ThreeDimensionalAudioPlayer::calculateSpeakerOutputs(
  const std::vector<SpeakerPosition>& speakers,
  double soundX, double soundY,
  double zoneWidth, double zoneHeight,
  double masterVolume)
{
  std::vector<SpeakerOutput> outputs;
  
  // Calculate maximum possible distance (diagonal of zone)
  double maxDistance = std::sqrt(zoneWidth * zoneWidth + zoneHeight * zoneHeight);
  
  // Calculate panning weights for all speakers
  auto panningWeights = calculatePanning(speakers, soundX, soundY, zoneWidth, zoneHeight);
  
  // Calculate the geometric center of all active speakers
  double speakerCenterX = 0.0;
  double speakerCenterY = 0.0;
  int activeSpeakerCount = 0;
  
  for(const auto& speaker : speakers)
  {
    if(!speaker.deviceId.empty())
    {
      speakerCenterX += speaker.x;
      speakerCenterY += speaker.y;
      activeSpeakerCount++;
    }
  }
  
  if(activeSpeakerCount > 0)
  {
    speakerCenterX /= activeSpeakerCount;
    speakerCenterY /= activeSpeakerCount;
  }
  
  // Calculate distance from sound to the speaker array center
  double soundToArrayCenterDist = calculateDistance(soundX, soundY, speakerCenterX, speakerCenterY);
  
  // First pass: calculate all distances
  std::vector<double> distances;
  
  for(const auto& speaker : speakers)
  {
    double distance = calculateDistance(soundX, soundY, speaker.x, speaker.y);
    distances.push_back(distance);
  }
  
  // Second pass: process each speaker
  for(size_t i = 0; i < speakers.size(); i++)
  {
    const auto& speaker = speakers[i];
    
    // Skip if no device assigned
    if(speaker.deviceId.empty())
      continue;
    
    double speakerDistance = distances[i];
    
    // Calculate attenuation based on inverse square law
    double attenuation = calculateAttenuation(speakerDistance, maxDistance);
    
    // Calculate how far this speaker is from the speaker array center
    double speakerToArrayCenterDist = calculateDistance(speaker.x, speaker.y, speakerCenterX, speakerCenterY);
    
    // Calculate delay based on the difference between:
    // - Distance from sound to speaker
    // - Distance from sound to array center
    // This makes the center position have 0 delay for all speakers
    double relativeDistance = speakerDistance - soundToArrayCenterDist;
    
    // Only apply positive delays (speakers farther than the array center point)
    double delay = (relativeDistance > 0) ? calculateDelay(relativeDistance) : 0.0;
    
    // Combine: master volume * speaker volume * attenuation * panning weight
    double finalVolume = masterVolume * speaker.volume * attenuation * panningWeights[i];
    
    // Clamp volume to [0, 1]
    finalVolume = std::clamp(finalVolume, 0.0, 1.0);
    
    // Only add if volume is significant
    if(finalVolume > 0.001)
    {
      SpeakerOutput output;
      output.deviceId = speaker.deviceId;
      output.channel = speaker.channel;
      output.volume = finalVolume;
      output.delay = delay;
      outputs.push_back(output);
    }
  }
  
  return outputs;
}


std::vector<SpeakerPosition> ThreeDimensionalAudioPlayer::parseZoneSpeakers(
  const std::string& speakersJson)
{
  std::vector<SpeakerPosition> speakers;
  
  try
  {
    if(speakersJson.empty())
      return speakers;
    
    json parsed = json::parse(speakersJson);
    if(!parsed.is_array())
      return speakers;
    
    for(const auto& speakerJson : parsed)
    {
      SpeakerPosition speaker;
      speaker.id = speakerJson.value("id", -1);
      speaker.x = speakerJson.value("x", 0.0);
      speaker.y = speakerJson.value("y", 0.0);
      speaker.label = speakerJson.value("label", "");
      speaker.deviceId = speakerJson.value("device", "");
      speaker.channel = speakerJson.value("channel", 0);
      speaker.volume = speakerJson.value("volume", 1.0);
      
      speakers.push_back(speaker);
    }
  }
  catch(const std::exception& e)
  {
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Error parsing speakers JSON: ") + e.what());
  }
  
  return speakers;
}

double ThreeDimensionalAudioPlayer::calculateDistance(double x1, double y1, 
                                                       double x2, double y2) const
{
  double dx = x2 - x1;
  double dy = y2 - y1;
  return std::sqrt(dx * dx + dy * dy);
}

double ThreeDimensionalAudioPlayer::calculateDelay(double distance) const
{
  // Delay in milliseconds = (distance in meters / speed of sound) * 1000
  return (distance / SPEED_OF_SOUND) * 1000.0;
}

double ThreeDimensionalAudioPlayer::calculateAttenuation(double distance, 
                                                          double maxDistance) const
{
  // Prevent division by zero
  if(distance < MIN_DISTANCE)
    distance = MIN_DISTANCE;
  
  // Inverse square law: intensity ∝ 1/d²
  // Normalize by maxDistance to keep values reasonable
  double normalizedDistance = distance / maxDistance;
  double attenuation = 1.0 / (1.0 + normalizedDistance * normalizedDistance * 4.0);
  
  return attenuation;
}

std::vector<double> ThreeDimensionalAudioPlayer::calculatePanning(
  const std::vector<SpeakerPosition>& speakers,
  double soundX, double soundY,
  double zoneWidth, double zoneHeight) const
{
  std::vector<double> weights(speakers.size(), 0.0);
  
  if(speakers.empty())
    return weights;
  
  // Separate speakers into front and rear rows
  std::vector<size_t> frontSpeakers;
  std::vector<size_t> rearSpeakers;
  
  for(size_t i = 0; i < speakers.size(); i++)
  {
    if(speakers[i].y < zoneHeight / 2.0)
      frontSpeakers.push_back(i);
    else
      rearSpeakers.push_back(i);
  }
  
  // Calculate front/rear balance (Y-axis)
  double yBalance = soundY / zoneHeight; // 0 = front, 1 = rear
  double frontWeight = 1.0 - yBalance;
  double rearWeight = yBalance;
  
  // Apply Y-axis panning to front speakers
  if(!frontSpeakers.empty())
  {
    // Calculate X-axis panning for front row
    for(size_t idx : frontSpeakers)
    {
      double speakerX = speakers[idx].x;
      double xDistance = std::abs(soundX - speakerX);
      double maxXDistance = zoneWidth;
      
      // Linear interpolation for X-axis
      double xWeight = 1.0 - (xDistance / maxXDistance);
      xWeight = std::clamp(xWeight, 0.0, 1.0);
      
      weights[idx] = frontWeight * xWeight;
    }
    
    // Normalize front weights
    double frontSum = 0.0;
    for(size_t idx : frontSpeakers)
      frontSum += weights[idx];
    if(frontSum > 0.0)
    {
      for(size_t idx : frontSpeakers)
        weights[idx] = (weights[idx] / frontSum) * frontWeight;
    }
  }
  
  // Apply Y-axis panning to rear speakers
  if(!rearSpeakers.empty())
  {
    // Calculate X-axis panning for rear row
    for(size_t idx : rearSpeakers)
    {
      double speakerX = speakers[idx].x;
      double xDistance = std::abs(soundX - speakerX);
      double maxXDistance = zoneWidth;
      
      // Linear interpolation for X-axis
      double xWeight = 1.0 - (xDistance / maxXDistance);
      xWeight = std::clamp(xWeight, 0.0, 1.0);
      
      weights[idx] = rearWeight * xWeight;
    }
    
    // Normalize rear weights
    double rearSum = 0.0;
    for(size_t idx : rearSpeakers)
      rearSum += weights[idx];
    if(rearSum > 0.0)
    {
      for(size_t idx : rearSpeakers)
        weights[idx] = (weights[idx] / rearSum) * rearWeight;
    }
  }
  
  return weights;
}
