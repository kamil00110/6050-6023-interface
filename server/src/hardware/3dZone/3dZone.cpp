#include "3dZone.hpp"
#include "list/3dZoneList.hpp"
#include "list/3dZoneListTableModel.hpp"
#include "../../world/getworld.hpp"
#include "../../world/world.hpp"
#include "../../core/attributes.hpp"
#include "../../core/method.tpp"
#include "../../core/objectproperty.tpp"
#include "../../utils/displayname.hpp"
#include "../../utils/audioenumerator.hpp"
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
      speaker["label"] = label; // Update label in case setup changed
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
  , width{this, "width", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,  // Changed from 10.0 to 1.0 (100cm)
      [this](double value)
      {
        speakersData.setValueInternal(updateSpeakerPositions(speakersData.value(), speakerSetup.value(), value, height.value()));
        return true;
      }}
  , height{this, "height", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,  // Changed from 10.0 to 1.0 (100cm)
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
{
  Attributes::addDisplayName(width, "Width (m)");
  Attributes::addMinMax(width, 0.1, 100.0);  // Max 100m = 10000cm
  Attributes::addEnabled(width, true);
  m_interfaceItems.add(width);
  
  Attributes::addDisplayName(height, "Height (m)");
  Attributes::addMinMax(height, 0.1, 100.0);  // Max 100m = 10000cm
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
  
  // Initialize with default speaker layout
  speakersData.setValueInternal(updateSpeakerPositions("", speakerSetup.value(), width.value(), height.value()));
  
  // Initialize audio devices list
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
