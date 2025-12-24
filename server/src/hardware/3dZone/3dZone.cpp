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
#include "../../utils/audioenumerator.hpp"  // ADD THIS
#include <nlohmann/json.hpp>

using nlohmann::json;

// ... existing initializeSpeakers function ...

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
  , getAudioDevices{*this, "get_audio_devices", MethodFlags::NoScript,  // ADD THIS
      []() -> std::string
      {
        // Enumerate audio devices and return as JSON
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
        
        return result.dump();
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
  
  // Add the get audio devices method
  Attributes::addDisplayName(getAudioDevices, "Get Audio Devices");
  Attributes::addEnabled(getAudioDevices, true);
  Attributes::addVisible(getAudioDevices, false); // Hidden from UI, used by client
  m_interfaceItems.add(getAudioDevices);
  
  // Initialize with default speaker layout
  speakersData.setValueInternal(initializeSpeakers(speakerSetup.value(), width.value(), height.value()));
  
  updateEnabled();
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
