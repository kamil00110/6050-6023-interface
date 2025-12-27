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
#include <sstream>
#include <iomanip>

using nlohmann::json;

constexpr double SPEED_OF_SOUND = 343.0; // meters per second
constexpr double MIN_DISTANCE = 0.01; // minimum distance to avoid division by zero

std::string ThreeDimensionalAudioPlayer::generateInstanceId()
{
  uint64_t id = m_nextInstanceId.fetch_add(1);
  std::ostringstream oss;
  oss << "sound_instance_" << std::setw(8) << std::setfill('0') << id;
  return oss.str();
}

std::vector<SpeakerQuad> ThreeDimensionalAudioPlayer::generateQuads(
  const std::vector<SpeakerPosition>& speakers) const
{
  std::vector<SpeakerQuad> quads;
  
  // Separate speakers into rows by Y position
  std::vector<std::vector<size_t>> rows;
  std::map<double, std::vector<size_t>> rowMap;
  
  for(size_t i = 0; i < speakers.size(); i++)
  {
    if(speakers[i].deviceId.empty())
      continue;
      
    // Group speakers by Y coordinate (with small tolerance)
    double yKey = std::round(speakers[i].y * 100.0) / 100.0;
    rowMap[yKey].push_back(i);
  }
  
  // Convert map to vector and sort each row by X
  for(auto& [yPos, indices] : rowMap)
  {
    std::sort(indices.begin(), indices.end(), 
      [&speakers](size_t a, size_t b) {
        return speakers[a].x < speakers[b].x;
      });
    rows.push_back(indices);
  }
  
  // Sort rows by Y position (front to back)
  std::sort(rows.begin(), rows.end(),
    [&speakers](const std::vector<size_t>& a, const std::vector<size_t>& b) {
      return speakers[a[0]].y < speakers[b[0]].y;
    });
  
  if(rows.size() < 2)
    return quads; // Need at least 2 rows
  
  // Generate quads between adjacent rows
  for(size_t rowIdx = 0; rowIdx < rows.size() - 1; rowIdx++)
  {
    const auto& frontRow = rows[rowIdx];
    const auto& backRow = rows[rowIdx + 1];
    
    // Create overlapping quads along X axis
    for(size_t frontIdx = 0; frontIdx < frontRow.size() - 1; frontIdx++)
    {
      size_t fl = frontRow[frontIdx];
      size_t fr = frontRow[frontIdx + 1];
      
      double frontLeftX = speakers[fl].x;
      double frontRightX = speakers[fr].x;
      double frontY = speakers[fl].y;
      
      for(size_t backIdx = 0; backIdx < backRow.size() - 1; backIdx++)
      {
        size_t bl = backRow[backIdx];
        size_t br = backRow[backIdx + 1];
        
        double backLeftX = speakers[bl].x;
        double backRightX = speakers[br].x;
        double backY = speakers[bl].y;
        
        double overlapLeft = std::max(frontLeftX, backLeftX);
        double overlapRight = std::min(frontRightX, backRightX);
        
        if(overlapRight > overlapLeft)
        {
          SpeakerQuad quad;
          quad.bottomLeft = fl;
          quad.bottomRight = fr;
          quad.topLeft = bl;
          quad.topRight = br;
          
          quad.minX = std::max(frontLeftX, backLeftX);
          quad.maxX = std::min(frontRightX, backRightX);
          quad.minY = frontY;
          quad.maxY = backY;
          
          quads.push_back(quad);
        }
      }
    }
  }
  
  return quads;
}

ThreeDimensionalAudioPlayer& ThreeDimensionalAudioPlayer::instance()
{
  static ThreeDimensionalAudioPlayer instance;
  return instance;
}

bool ThreeDimensionalAudioPlayer::playSound(World& world, const std::string& zoneId, 
                                            double x, double y, 
                                            const std::string& instanceId,
                                            const std::string& soundId, 
                                            double volume)
{
  try
  {
    // Check if instance ID is already in use
    if(m_activeSounds.count(instanceId) > 0)
    {
      Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
        std::string("ERROR: Instance ID '") + instanceId + "' is already playing. " +
        "Use stopSound() first or choose a different instance ID.");
      return false;
    }
    
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Attempting to play sound '") + soundId + "' with instance ID '" + 
      instanceId + "' in zone '" + zoneId + "' at position (" + 
      std::to_string(x) + ", " + std::to_string(y) + ")");
    
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
    
    // Load audio file with instance ID
    if(!backend.loadAudioFile(audioFilePath.string(), instanceId))
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
    
    if(!backend.playSound(instanceId, streamConfigs, looping, speed))
    {
      Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
        std::string("Failed to start playback"));
      backend.unloadAudioFile(instanceId);
      return false;
    }
    
    // Create active sound entry
    ActiveSound activeSound;
    activeSound.instanceId = instanceId;
    activeSound.soundId = soundId;
    activeSound.zoneId = zoneId;
    activeSound.x = x;
    activeSound.y = y;
    activeSound.volume = volume;
    activeSound.looping = looping;
    activeSound.speed = speed;
    activeSound.speakers = speakers;
    activeSound.speakerOutputs = outputs;
    activeSound.startTime = 0; // TODO: Set to current time in milliseconds
    activeSound.zoneWidth = zoneWidth;
    activeSound.zoneHeight = zoneHeight;
    
    m_activeSounds[instanceId] = activeSound;
    
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Sound instance '") + instanceId + "' started " + 
      "(sound: " + soundId + ", " + (activeSound.looping ? "looping)" : "one-shot)"));
    
    return true;
  }
  catch(const std::exception& e)
  {
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Error playing sound: ") + e.what());
    return false;
  }
}

bool ThreeDimensionalAudioPlayer::stopSound(const std::string& instanceId)
{
  auto it = m_activeSounds.find(instanceId);
  if(it == m_activeSounds.end())
  {
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Sound instance not playing: ") + instanceId);
    return false;
  }
  
  Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
    std::string("Stopping sound instance: ") + instanceId + " (sound: " + it->second.soundId + ")");
  
  // Stop playback through WASAPI backend
  WASAPIAudioBackend::instance().stopSound(instanceId);
  
  // Unload audio file
  WASAPIAudioBackend::instance().unloadAudioFile(instanceId);
  
  m_activeSounds.erase(it);
  return true;
}

bool ThreeDimensionalAudioPlayer::moveSound(const std::string& instanceId, double newX, double newY)
{
  auto it = m_activeSounds.find(instanceId);
  if(it == m_activeSounds.end())
  {
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Cannot move sound: instance not playing: ") + instanceId);
    return false;
  }
  
  ActiveSound& activeSound = it->second;
  
  // Validate new position is within zone
  if(newX < 0 || newX > activeSound.zoneWidth || newY < 0 || newY > activeSound.zoneHeight)
  {
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Cannot move sound: new position out of zone bounds"));
    return false;
  }
  
  Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
    std::string("Moving sound instance '") + instanceId + "' from (" + 
    std::to_string(activeSound.x) + ", " + std::to_string(activeSound.y) + ") to (" +
    std::to_string(newX) + ", " + std::to_string(newY) + ")");
  
  // Update position
  activeSound.x = newX;
  activeSound.y = newY;
  
  // Recalculate speaker outputs
  return recalculateSpeakerOutputs(activeSound);
}

bool ThreeDimensionalAudioPlayer::updateSoundVolume(const std::string& instanceId, double newVolume)
{
  auto it = m_activeSounds.find(instanceId);
  if(it == m_activeSounds.end())
  {
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Cannot update volume: instance not playing: ") + instanceId);
    return false;
  }
  
  ActiveSound& activeSound = it->second;
  
  // Clamp volume
  newVolume = std::clamp(newVolume, 0.0, 1.0);
  
  Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
    std::string("Updating volume for instance '") + instanceId + "' from " + 
    std::to_string(activeSound.volume) + " to " + std::to_string(newVolume));
  
  // Update volume
  activeSound.volume = newVolume;
  
  // Recalculate speaker outputs
  return recalculateSpeakerOutputs(activeSound);
}

bool ThreeDimensionalAudioPlayer::recalculateSpeakerOutputs(ActiveSound& activeSound)
{
  // Recalculate speaker outputs with new position/volume
  auto newOutputs = calculateSpeakerOutputs(
    activeSound.speakers, 
    activeSound.x, 
    activeSound.y, 
    activeSound.zoneWidth, 
    activeSound.zoneHeight, 
    activeSound.volume
  );
  
  // Convert to stream configs
  std::vector<AudioStreamConfig> streamConfigs;
  for(const auto& output : newOutputs)
  {
    AudioStreamConfig config;
    config.deviceId = output.deviceId;
    config.channel = output.channel;
    config.volume = output.volume;
    config.delay = output.delay;
    streamConfigs.push_back(config);
  }
  
  // Update the playback through WASAPI
  auto& backend = WASAPIAudioBackend::instance();
  
  // Update audio streams with new parameters
  if(!backend.updateSound(activeSound.instanceId, streamConfigs))
  {
    Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
      std::string("Failed to update sound playback parameters"));
    return false;
  }
  
  // Update stored outputs
  activeSound.speakerOutputs = newOutputs;
  
  return true;
}

void ThreeDimensionalAudioPlayer::stopAllInstancesOfSound(const std::string& soundId)
{
  std::vector<std::string> instancesToStop;
  
  // Find all instances of this sound
  for(const auto& [instanceId, activeSound] : m_activeSounds)
  {
    if(activeSound.soundId == soundId)
    {
      instancesToStop.push_back(instanceId);
    }
  }
  
  Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
    std::string("Stopping ") + std::to_string(instancesToStop.size()) + 
    " instance(s) of sound: " + soundId);
  
  // Stop each instance
  for(const auto& instanceId : instancesToStop)
  {
    stopSound(instanceId);
  }
}

void ThreeDimensionalAudioPlayer::stopAllSounds()
{
  Log::log(std::string("3DAudioPlayer"), LogMessage::I1006_X,
    std::string("Stopping all sounds (") + std::to_string(m_activeSounds.size()) + " active instances)");
  
  // Stop all sounds through WASAPI backend
  WASAPIAudioBackend::instance().stopAllSounds();
  
  // Unload all audio files
  for(const auto& [instanceId, _] : m_activeSounds)
  {
    WASAPIAudioBackend::instance().unloadAudioFile(instanceId);
  }
  
  m_activeSounds.clear();
}

std::vector<std::string> ThreeDimensionalAudioPlayer::getActiveSoundInstances() const
{
  std::vector<std::string> instances;
  for(const auto& [instanceId, _] : m_activeSounds)
  {
    instances.push_back(instanceId);
  }
  return instances;
}

std::vector<PlayingSoundInfo> ThreeDimensionalAudioPlayer::getAllPlayingSounds() const
{
  std::vector<PlayingSoundInfo> playingSounds;
  
  for(const auto& [instanceId, activeSound] : m_activeSounds)
  {
    PlayingSoundInfo info;
    info.instanceId = activeSound.instanceId;
    info.soundId = activeSound.soundId;
    info.zoneId = activeSound.zoneId;
    info.x = activeSound.x;
    info.y = activeSound.y;
    info.volume = activeSound.volume;
    info.looping = activeSound.looping;
    info.speed = activeSound.speed;
    info.startTime = activeSound.startTime;
    
    playingSounds.push_back(info);
  }
  
  return playingSounds;
}

bool ThreeDimensionalAudioPlayer::isSoundInstancePlaying(const std::string& instanceId) const
{
  return m_activeSounds.count(instanceId) > 0;
}

int ThreeDimensionalAudioPlayer::countPlayingInstancesOfSound(const std::string& soundId) const
{
  int count = 0;
  for(const auto& [instanceId, activeSound] : m_activeSounds)
  {
    if(activeSound.soundId == soundId)
    {
      count++;
    }
  }
  return count;
}

bool ThreeDimensionalAudioPlayer::getSoundInfo(const std::string& instanceId, 
                                                PlayingSoundInfo& info) const
{
  auto it = m_activeSounds.find(instanceId);
  if(it == m_activeSounds.end())
    return false;
  
  const ActiveSound& activeSound = it->second;
  info.instanceId = activeSound.instanceId;
  info.soundId = activeSound.soundId;
  info.zoneId = activeSound.zoneId;
  info.x = activeSound.x;
  info.y = activeSound.y;
  info.volume = activeSound.volume;
  info.looping = activeSound.looping;
  info.speed = activeSound.speed;
  info.startTime = activeSound.startTime;
  
  return true;
}

std::vector<SpeakerOutput> ThreeDimensionalAudioPlayer::calculateSpeakerOutputs(
  const std::vector<SpeakerPosition>& speakers,
  double soundX, double soundY,
  double zoneWidth, double zoneHeight,
  double masterVolume)
{
  std::vector<SpeakerOutput> outputs;
  
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
    
    // Calculate delay - speakers farther from sound get positive delay
    double relativeDistance = soundToArrayCenterDist - speakerDistance;
    double delay = (relativeDistance > 0) ? calculateDelay(relativeDistance) : 0.0;
    
    // Combine: master volume * speaker volume * panning weight
    double finalVolume = masterVolume * speaker.volume * panningWeights[i];
    
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
  return (distance / SPEED_OF_SOUND * 1000);
}

double ThreeDimensionalAudioPlayer::calculateAttenuation(double distance, 
                                                          double maxDistance) const
{
  if(distance < MIN_DISTANCE)
    distance = MIN_DISTANCE;
  
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
  
  int frontSpeakerCount = 0;
  int backSpeakerCount = 0;
  
  for(const auto& speaker : speakers)
  {
    if(speaker.deviceId.empty())
      continue;
      
    if(speaker.y < zoneHeight / 2.0)
      frontSpeakerCount++;
    else
      backSpeakerCount++;
  }
  
  if((frontSpeakerCount <= 2 && backSpeakerCount <= 2) || 
     (frontSpeakerCount == 0 || backSpeakerCount == 0))
  {
    return calculateSimplePanning(speakers, soundX, soundY, zoneWidth, zoneHeight);
  }
  
  auto quads = generateQuads(speakers);
  
  if(quads.empty())
  {
    return calculateSimplePanning(speakers, soundX, soundY, zoneWidth, zoneHeight);
  }
  
  const SpeakerQuad* selectedQuad = nullptr;
  
  for(const auto& quad : quads)
  {
    if(quad.containsPoint(soundX, soundY))
    {
      selectedQuad = &quad;
      break;
    }
  }
  
  if(!selectedQuad)
  {
    double minDist = std::numeric_limits<double>::max();
    
    for(const auto& quad : quads)
    {
      double centerX = (quad.minX + quad.maxX) / 2.0;
      double centerY = (quad.minY + quad.maxY) / 2.0;
      double dist = std::sqrt(std::pow(soundX - centerX, 2) + std::pow(soundY - centerY, 2));
      
      if(dist < minDist)
      {
        minDist = dist;
        selectedQuad = &quad;
      }
    }
  }
  
  if(selectedQuad)
  {
    weights = calculateQuadPanning(speakers, *selectedQuad, soundX, soundY);
  }
  
  return weights;
}

std::vector<double> ThreeDimensionalAudioPlayer::calculateQuadPanning(
  const std::vector<SpeakerPosition>& speakers,
  const SpeakerQuad& quad,
  double soundX, double soundY) const
{
  std::vector<double> weights(speakers.size(), 0.0);
  
  double normX = (soundX - quad.minX) / (quad.maxX - quad.minX);
  double normY = (soundY - quad.minY) / (quad.maxY - quad.minY);
  normX = std::clamp(normX, 0.0, 1.0);
  normY = std::clamp(normY, 0.0, 1.0);
  
  constexpr double PI = 3.14159265358979323846;
  
  double bottomAngle = normX * PI / 2.0;
  double bottomLeft = std::cos(bottomAngle);
  double bottomRight = std::sin(bottomAngle);
  
  double topAngle = normX * PI / 2.0;
  double topLeft = std::cos(topAngle);
  double topRight = std::sin(topAngle);
  
  double yAngle = normY * PI / 2.0;
  double frontWeight = std::cos(yAngle);
  double backWeight = std::sin(yAngle);
  
  weights[quad.bottomLeft] = bottomLeft * frontWeight;
  weights[quad.bottomRight] = bottomRight * frontWeight;
  weights[quad.topLeft] = topLeft * backWeight;
  weights[quad.topRight] = topRight * backWeight;
  
  return weights;
}

std::vector<double> ThreeDimensionalAudioPlayer::calculateSimplePanning(
  const std::vector<SpeakerPosition>& speakers,
  double soundX, double soundY,
  double /*zoneWidth*/, double zoneHeight) const
{
  std::vector<double> weights(speakers.size(), 0.0);
  
  std::vector<size_t> frontSpeakers;
  std::vector<size_t> rearSpeakers;
  
  for(size_t i = 0; i < speakers.size(); i++)
  {
    if(speakers[i].deviceId.empty())
      continue;
      
    if(speakers[i].y < zoneHeight / 2.0)
      frontSpeakers.push_back(i);
    else
      rearSpeakers.push_back(i);
  }
  
  double yBalance = soundY / zoneHeight;
  constexpr double PI = 3.14159265358979323846;
  double yAngle = yBalance * PI / 2.0;
  double frontWeight = std::cos(yAngle);
  double rearWeight = std::sin(yAngle);
  
  if(!frontSpeakers.empty())
  {
    if(frontSpeakers.size() == 2)
    {
      size_t leftIdx = frontSpeakers[0];
      size_t rightIdx = frontSpeakers[1];
      
      if(speakers[leftIdx].x > speakers[rightIdx].x)
        std::swap(leftIdx, rightIdx);
      
      double leftX = speakers[leftIdx].x;
      double rightX = speakers[rightIdx].x;
      double speakerSpan = rightX - leftX;
      
      if(speakerSpan > 0.001)
      {
        double pan = (soundX - leftX) / speakerSpan;
        pan = std::clamp(pan, 0.0, 1.0);
        
        double angle = pan * PI / 2.0;
        weights[leftIdx] = std::cos(angle) * frontWeight;
        weights[rightIdx] = std::sin(angle) * frontWeight;
      }
      else
      {
        weights[leftIdx] = frontWeight / std::sqrt(2.0);
        weights[rightIdx] = frontWeight / std::sqrt(2.0);
      }
    }
    else if(frontSpeakers.size() == 1)
    {
      weights[frontSpeakers[0]] = frontWeight;
    }
  }
  
  if(!rearSpeakers.empty())
  {
    if(rearSpeakers.size() == 2)
    {
      size_t leftIdx = rearSpeakers[0];
      size_t rightIdx = rearSpeakers[1];
      
      if(speakers[leftIdx].x > speakers[rightIdx].x)
        std::swap(leftIdx, rightIdx);
      
      double leftX = speakers[leftIdx].x;
      double rightX = speakers[rightIdx].x;
      double speakerSpan = rightX - leftX;
      
      if(speakerSpan > 0.001)
      {
        double pan = (soundX - leftX) / speakerSpan;
        pan = std::clamp(pan, 0.0, 1.0);
        
        double angle = pan * PI / 2.0;
        weights[leftIdx] = std::cos(angle) * rearWeight;
        weights[rightIdx] = std::sin(angle) * rearWeight;
      }
      else
      {
        weights[leftIdx] = rearWeight / std::sqrt(2.0);
        weights[rightIdx] = rearWeight / std::sqrt(2.0);
      }
    }
    else if(rearSpeakers.size() == 1)
    {
      weights[rearSpeakers[0]] = rearWeight;
    }
  }
  
  return weights;
}
