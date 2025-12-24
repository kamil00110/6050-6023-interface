/**
 * server/src/hardware/3dZone/3dZone.cpp
 */
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

// IMPORTANT: Declare this function BEFORE the constructor
static std::string initializeSpeakers(SpeakerSetup setup, double width, double height)
{
  const int count = static_cast<int>(setup);
  json speakers = json::array();
  
  if(count == 4)
  {
    // 4 speakers: one at each corner
    speakers.push_back({
      {"id", 0}, {"x", 0.0}, {"y", 0.0},
      {"label", "Front Left"}, {"device", ""}, {"channel", 0}, {"volume", 1.0}
    });
    speakers.push_back({
      {"id", 1}, {"x", width}, {"y", 0.0},
      {"label", "Front Right"}, {"device", ""}, {"channel", 1}, {"volume", 1.0}
    });
    speakers.push_back({
      {"id", 2}, {"x", width}, {"y", height},
      {"label", "Rear Right"}, {"device", ""}, {"channel", 2}, {"volume", 1.0}
    });
    speakers.push_back({
      {"id", 3}, {"x", 0.0}, {"y", height},
      {"label", "Rear Left"}, {"device", ""}, {"channel", 3}, {"volume", 1.0}
    });
  }
  else if(count == 6)
  {
    // 6 speakers: corners + 2 on top/bottom (horizontal edges only)
    speakers.push_back({
      {"id", 0}, {"x", 0.0}, {"y", 0.0},
      {"label", "Front Left"}, {"device", ""}, {"channel", 0}, {"volume", 1.0}
    });
    speakers.push_back({
      {"id", 1}, {"x", width / 2.0}, {"y", 0.0},
      {"label", "Front Center"}, {"device", ""}, {"channel", 1}, {"volume", 1.0}
    });
    speakers.push_back({
      {"id", 2}, {"x", width}, {"y", 0.0},
      {"label", "Front Right"}, {"device", ""}, {"channel", 2}, {"volume", 1.0}
    });
    speakers.push_back({
      {"id", 3}, {"x", width}, {"y", height},
      {"label", "Rear Right"}, {"device", ""}, {"channel", 3}, {"volume", 1.0}
    });
    speakers.push_back({
      {"id", 4}, {"x", width / 2.0}, {"y", height},
      {"label", "Rear Center"}, {"device", ""}, {"channel", 4}, {"volume", 1.0}
    });
    speakers.push_back({
      {"id", 5}, {"x", 0.0}, {"y", height},
      {"label", "Rear Left"}, {"device", ""}, {"channel", 5}, {"volume", 1.0}
    });
  }
  else if(count == 8)
  {
    // 8 speakers: 4 on top, 4 on bottom (horizontal edges only)
    for(int i = 0; i < 4; i++)
    {
      speakers.push_back({
        {"id", i},
        {"x", width * i / 3.0},
        {"y", 0.0},
        {"label", "Front " + std::to_string(i + 1)},
        {"device", ""},
        {"channel", i},
        {"volume", 1.0}
      });
    }
    for(int i = 0; i < 4; i++)
    {
      speakers.push_back({
        {"id", i + 4},
        {"x", width * i / 3.0},
        {"y", height},
        {"label", "Rear " + std::to_string(i + 1)},
        {"device", ""},
        {"channel", i + 4},
        {"volume", 1.0}
      });
    }
  }
  else if(count == 10)
  {
    // 10 speakers: 5 on top, 5 on bottom (horizontal edges only)
    for(int i = 0; i < 5; i++)
    {
      speakers.push_back({
        {"id", i},
        {"x", width * i / 4.0},
        {"y", 0.0},
        {"label", "Front " + std::to_string(i + 1)},
        {"device", ""},
        {"channel", i},
        {"volume", 1.0}
      });
    }
    for(int i = 0; i < 5; i++)
    {
      speakers.push_back({
        {"id", i + 5},
        {"x", width * i / 4.0},
        {"y", height},
        {"label", "Rear " + std::to_string(i + 1)},
        {"device", ""},
        {"channel", i + 5},
        {"volume", 1.0}
      });
    }
  }
  
  return speakers.dump();
}

ThreeDZone::ThreeDZone(World& world, std::string_view _id)
  : IdObject(world, _id)
  , width{this, "width", 10.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value)
      {
        speakersData.setValueInternal(initializeSpeakers(speakerSetup.value(), value, height.value()));
        return true;
      }}
  , height{this, "height", 10.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value)
      {
        speakersData.setValueInternal(initializeSpeakers(speakerSetup.value(), width.value(), value));
        return true;
      }}
  , speakerSetup{this, "speaker_setup", SpeakerSetup::Quadraphonic, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](SpeakerSetup value)
      {
        speakersData.setValueInternal(initializeSpeakers(value, width.value(), height.value()));
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
  Attributes::addMinMax(width, 0.1, 1000.0);
  Attributes::addEnabled(width, true);
  m_interfaceItems.add(width);
  
  Attributes::addDisplayName(height, "Height (m)");
  Attributes::addMinMax(height, 0.1, 1000.0);
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
  
  // Add audio devices JSON property
  Attributes::addDisplayName(audioDevicesJson, "Audio Devices (JSON)");
  Attributes::addVisible(audioDevicesJson, false); // Hidden, used by client
  m_interfaceItems.add(audioDevicesJson);
  
  // Add refresh method
  Attributes::addDisplayName(refreshAudioDevicesList, "Refresh Audio Devices");
  Attributes::addVisible(refreshAudioDevicesList, false);
  m_interfaceItems.add(refreshAudioDevicesList);
  
  // Initialize with default speaker layout
  speakersData.setValueInternal(initializeSpeakers(speakerSetup.value(), width.value(), height.value()));
  
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
