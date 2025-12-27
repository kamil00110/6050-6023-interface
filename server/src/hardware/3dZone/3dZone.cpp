#include "3dZone.hpp"
#include "list/3dZoneList.hpp"
#include "list/3dZoneListTableModel.hpp"
#include "../3dSound/3dSound.hpp"
#include "../3dSound/3dAudioPlayer.hpp"
#include "../3dSound/list/3dSoundList.hpp"
#include "../../world/getworld.hpp"
#include "../../world/world.hpp"
#include "../../core/attributes.hpp"
#include "../../core/method.tpp"
#include "../../core/objectproperty.tpp"
#include "../../utils/displayname.hpp"
#include "../../utils/audioenumerator.hpp"
#include "../../log/log.hpp"
#include "../../log/logmessageexception.hpp"
#include <nlohmann/json.hpp>

using nlohmann::json;

// Helper to update speaker positions without losing configuration
static std::string updateSpeakerPositions(const std::string& existingSpeakersJson, SpeakerSetup setup, double width, double height)
{
  const int count = static_cast<int>(setup);
  json speakers;
  
  // Try to parse existing speakers to preserve configuration
  std::map<int, json> existingSpeakers;
  try
  {
    if(!existingSpeakersJson.empty())
    {
      json existing = json::parse(existingSpeakersJson);
      if(existing.is_array())
      {
        for(const auto& speaker : existing)
        {
          if(speaker.contains("id"))
          {
            existingSpeakers[speaker["id"].get<int>()] = speaker;
          }
        }
      }
    }
  }
  catch(...)
  {
    // If parsing fails, start fresh
  }
  
  speakers = json::array();
  
  // Helper lambda to create or update speaker
  auto addSpeaker = [&](int id, double x, double y, const std::string& label, int defaultChannel)
  {
    json speaker;
    
    // Check if speaker already exists
    if(existingSpeakers.count(id))
    {
      // Preserve existing configuration, just update position
      speaker = existingSpeakers[id];
      speaker["x"] = x;
      speaker["y"] = y;
      speaker["label"] = label;
    }
    else
    {
      // Create new speaker
      speaker = {
        {"id", id},
        {"x", x},
        {"y", y},
        {"label", label},
        {"device", ""},
        {"channel", defaultChannel},
        {"volume", 1.0}
      };
    }
    
    speakers.push_back(speaker);
  };
  
  if(count == 4)
  {
    addSpeaker(0, 0.0, 0.0, "Front Left", 0);
    addSpeaker(1, width, 0.0, "Front Right", 1);
    addSpeaker(2, width, height, "Rear Right", 2);
    addSpeaker(3, 0.0, height, "Rear Left", 3);
  }
  else if(count == 6)
  {
    addSpeaker(0, 0.0, 0.0, "Front Left", 0);
    addSpeaker(1, width / 2.0, 0.0, "Front Center", 1);
    addSpeaker(2, width, 0.0, "Front Right", 2);
    addSpeaker(3, width, height, "Rear Right", 3);
    addSpeaker(4, width / 2.0, height, "Rear Center", 4);
    addSpeaker(5, 0.0, height, "Rear Left", 5);
  }
  else if(count == 8)
  {
    for(int i = 0; i < 4; i++)
    {
      addSpeaker(i, width * i / 3.0, 0.0, "Front " + std::to_string(i + 1), i);
    }
    for(int i = 0; i < 4; i++)
    {
      addSpeaker(i + 4, width * i / 3.0, height, "Rear " + std::to_string(i + 1), i + 4);
    }
  }
  else if(count == 10)
  {
    for(int i = 0; i < 5; i++)
    {
      addSpeaker(i, width * i / 4.0, 0.0, "Front " + std::to_string(i + 1), i);
    }
    for(int i = 0; i < 5; i++)
    {
      addSpeaker(i + 5, width * i / 4.0, height, "Rear " + std::to_string(i + 1), i + 5);
    }
  }
  
  return speakers.dump();
}

ThreeDZone::ThreeDZone(World& world, std::string_view _id)
  : IdObject(world, _id)
  , width{this, "width", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value)
      {
        speakersData.setValueInternal(updateSpeakerPositions(speakersData.value(), speakerSetup.value(), value, height.value()));
        return true;
      }}
  , height{this, "height", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value)
      {
        speakersData.setValueInternal(updateSpeakerPositions(speakersData.value(), speakerSetup.value(), width.value(), value));
        return true;
      }}
  , speakerSetup{this, "speaker_setup", SpeakerSetup::Quadraphonic, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](SpeakerSetup value)
      {
        speakersData.setValueInternal(updateSpeakerPositions(speakersData.value(), value, width.value(), height.value()));
        return true;
      }}
  , speakersData{this, "speakers_data", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , audioDevicesJson{this, "audio_devices_json", "", PropertyFlags::ReadOnly | PropertyFlags::NoStore}
  , refreshAudioDevicesList{*this, "refresh_audio_devices", MethodFlags::NoScript,
      [this]()
      {
        refreshAudioDevices();
      }}
  , testSoundAtPosition{*this, "test_sound_at_position", MethodFlags::NoScript,
    [this](const std::string& coordsStr)
    {
      // Parse the coordinates from the string
      double x = 0.0, y = 0.0;
      
      // Simple parsing: "x,y"
      size_t commaPos = coordsStr.find(',');
      if(commaPos != std::string::npos)
      {
        try
        {
          x = std::stod(coordsStr.substr(0, commaPos));
          y = std::stod(coordsStr.substr(commaPos + 1));
        }
        catch(...)
        {
          Log::log(*this, LogMessage::I1006_X,
            std::string("Failed to parse coordinates: ") + coordsStr);
          return;
        }
      }
      
      Log::log(*this, LogMessage::I1006_X, 
        std::string("Test sound at position: x=") + std::to_string(x) + 
        ", y=" + std::to_string(y));
    
      // Find a sound file to play for testing
      World& w = getWorld(*this);
      auto soundsList = w.threeDSounds.value();
      
      if(!soundsList || soundsList->empty())
      {
        Log::log(*this, LogMessage::I1006_X,
          std::string("No sound files available for testing"));
        return;
      }
      
      // Use the first available sound file
      auto firstSound = soundsList->front();
      if(!firstSound)
      {
        Log::log(*this, LogMessage::I1006_X,
          std::string("Invalid sound object"));
        return;
      }
      
      // Generate a unique test instance ID
      std::string instanceId = "test_sound_" + id.value() + "_" + 
                               std::to_string(static_cast<int>(x)) + "_" + 
                               std::to_string(static_cast<int>(y));
      
      // Play the sound at the specified position
      bool success = ThreeDimensionalAudioPlayer::instance().playSound(
        w,
        id.value(),
        x,
        y,
        instanceId,
        firstSound->id.value(),
        1.0
      );
      
      if(success)
      {
        Log::log(*this, LogMessage::I1006_X,
          std::string("Playing test sound '") + firstSound->id.value() + 
          "' with instance ID '" + instanceId + "' at (" + 
          std::to_string(x) + ", " + std::to_string(y) + ")");
      }
      else
      {
        Log::log(*this, LogMessage::I1006_X,
          std::string("Failed to play test sound - instance may already be playing"));
      }
    }}
  , playSoundAtPosition{*this, "play_sound_at_position", MethodFlags::NoScript,
      [this](double x, double y, const std::string& instanceId, const std::string& soundId)
      {
        World& w = getWorld(*this);
        auto& player = ThreeDimensionalAudioPlayer::instance();
        
        // Check if this instance is already playing
        if(player.isSoundInstancePlaying(instanceId))
        {
          // Sound is already playing - just move it to new position
          bool success = player.moveSound(instanceId, x, y);
          
          if(success)
          {
            Log::log(*this, LogMessage::I1006_X,
              std::string("Moved existing sound instance '") + instanceId + 
              "' to position (" + std::to_string(x) + ", " + std::to_string(y) + ")");
          }
          else
          {
            Log::log(*this, LogMessage::I1006_X,
              std::string("Failed to move sound '") + instanceId + 
              "' - position may be out of bounds");
          }
        }
        else
        {
          // Sound not playing - start it at this position
          bool success = player.playSound(
            w,
            id.value(),
            x,
            y,
            instanceId,
            soundId,
            1.0
          );
          
          if(success)
          {
            Log::log(*this, LogMessage::I1006_X,
              std::string("Started new sound '") + soundId + "' with instance ID '" + 
              instanceId + "' at (" + std::to_string(x) + ", " + std::to_string(y) + ")");
          }
          else
          {
            Log::log(*this, LogMessage::I1006_X,
              std::string("Failed to start sound '") + soundId + "' - sound not found");

          }
        }
      }}
  , updateSoundVolume{*this, "update_sound_volume", MethodFlags::NoScript,
      [this](const std::string& instanceId, double newVolume)
      {
        bool success = ThreeDimensionalAudioPlayer::instance().updateSoundVolume(
          instanceId,
          newVolume
        );
        
        if(success)
        {
          Log::log(*this, LogMessage::I1006_X,
            std::string("Updated volume for sound instance '") + instanceId + 
            "' to " + std::to_string(newVolume));
        }
        else
        {
          Log::log(*this, LogMessage::I1006_X,
            std::string("Failed to update volume for '") + instanceId + 
            "' - instance not playing");
        }
      }}
  , stopSoundInstance{*this, "stop_sound_instance", MethodFlags::NoScript,
      [this](const std::string& instanceId)
      {
        bool success = ThreeDimensionalAudioPlayer::instance().stopSound(instanceId);
        
        if(success)
        {
          Log::log(*this, LogMessage::I1006_X,
            std::string("Stopped sound instance: ") + instanceId);
        }
        else
        {
          Log::log(*this, LogMessage::I1006_X,
            std::string("Failed to stop sound '") + instanceId + 
            "' - instance not playing");
        }
      }}
  , getPlayingSounds{*this, "get_playing_sounds", MethodFlags::NoScript,
      [this]() -> std::string
      {
        auto playingSounds = ThreeDimensionalAudioPlayer::instance().getAllPlayingSounds();
        
        json result = json::array();
        
        for(const auto& sound : playingSounds)
        {
          // Only include sounds from this zone
          if(sound.zoneId == id.value())
          {
            json soundJson;
            soundJson["instanceId"] = sound.instanceId;
            soundJson["soundId"] = sound.soundId;
            soundJson["x"] = sound.x;
            soundJson["y"] = sound.y;
            soundJson["volume"] = sound.volume;
            soundJson["looping"] = sound.looping;
            soundJson["speed"] = sound.speed;
            
            result.push_back(soundJson);
          }
        }
        
        Log::log(*this, LogMessage::I1006_X,
          std::string("Found ") + std::to_string(result.size()) + 
          " playing sounds in zone '" + id.value() + "'");
        
        return result.dump();
      }}
{
  Attributes::addDisplayName(width, "Width (m)");
  Attributes::addMinMax(width, 0.1, 100.0);
  Attributes::addStep(width, 0.1);
  Attributes::addEnabled(width, true);
  m_interfaceItems.add(width);
  
  Attributes::addDisplayName(height, "Height (m)");
  Attributes::addMinMax(height, 0.1, 100.0);
  Attributes::addStep(height, 0.1);
  Attributes::addEnabled(height, true);
  m_interfaceItems.add(height);
  
  Attributes::addDisplayName(speakerSetup, "Speaker Setup");
  Attributes::addValues(speakerSetup, speakerSetupValues);
  Attributes::addEnabled(speakerSetup, true);
  m_interfaceItems.add(speakerSetup);
  
  Attributes::addDisplayName(speakersData, "Speakers Configuration (JSON)");
  Attributes::addEnabled(speakersData, true);
  Attributes::addVisible(speakersData, false);
  m_interfaceItems.add(speakersData);
  
  Attributes::addDisplayName(audioDevicesJson, "Audio Devices (JSON)");
  Attributes::addVisible(audioDevicesJson, false);
  m_interfaceItems.add(audioDevicesJson);
  
  Attributes::addDisplayName(refreshAudioDevicesList, "Refresh Audio Devices");
  Attributes::addVisible(refreshAudioDevicesList, false);
  m_interfaceItems.add(refreshAudioDevicesList);
  
  Attributes::addDisplayName(testSoundAtPosition, "Test Sound At Position");
  Attributes::addVisible(testSoundAtPosition, false);
  m_interfaceItems.add(testSoundAtPosition);
  
  Attributes::addDisplayName(playSoundAtPosition, "Play Sound At Position");
  Attributes::addVisible(playSoundAtPosition, false);
  m_interfaceItems.add(playSoundAtPosition);
  
  Attributes::addDisplayName(updateSoundVolume, "Update Sound Volume");
  Attributes::addVisible(updateSoundVolume, false);
  m_interfaceItems.add(updateSoundVolume);
  
  Attributes::addDisplayName(stopSoundInstance, "Stop Sound Instance");
  Attributes::addVisible(stopSoundInstance, false);
  m_interfaceItems.add(stopSoundInstance);
  
  Attributes::addDisplayName(getPlayingSounds, "Get Playing Sounds");
  Attributes::addVisible(getPlayingSounds, false);
  m_interfaceItems.add(getPlayingSounds);
  
  speakersData.setValueInternal(updateSpeakerPositions("", speakerSetup.value(), width.value(), height.value()));
  refreshAudioDevices();
  
  updateEnabled();
}

void ThreeDZone::refreshAudioDevices()
{
  auto devices = AudioEnumerator::enumerateDevices();
  
  json result = json::array();
  for(const auto& device : devices)
  {
    json deviceJson;
    deviceJson["id"] = device.deviceId;
    deviceJson["name"] = device.deviceName;
    deviceJson["channelCount"] = device.channelCount;
    deviceJson["isDefault"] = device.isDefault;
    
    json channelsJson = json::array();
    for(const auto& channel : device.channels)
    {
      json channelJson;
      channelJson["index"] = channel.channelIndex;
      channelJson["name"] = channel.channelName;
      channelsJson.push_back(channelJson);
    }
    deviceJson["channels"] = channelsJson;
    
    result.push_back(deviceJson);
  }
  
  audioDevicesJson.setValueInternal(result.dump());
}

void ThreeDZone::addToWorld()
{
  IdObject::addToWorld();
  if(auto list = getWorld(*this).threeDZones.value())
    list->addObject(shared_ptr<ThreeDZone>());
}

void ThreeDZone::loaded()
{
  IdObject::loaded();
  updateEnabled();
}

void ThreeDZone::destroying()
{
  if(auto list = getWorld(*this).threeDZones.value())
    list->removeObject(shared_ptr<ThreeDZone>());
  IdObject::destroying();
}

void ThreeDZone::worldEvent(WorldState state, WorldEvent event)
{
  IdObject::worldEvent(state, event);
  updateEnabled();
}

void ThreeDZone::updateEnabled()
{
  const bool editable = contains(getWorld(*this).state.value(), WorldState::Edit);
  Attributes::setEnabled(width, editable);
  Attributes::setEnabled(height, editable);
  Attributes::setEnabled(speakerSetup, editable);
  Attributes::setEnabled(speakersData, editable);
}
