#include "3dZone.hpp"
#include "list/3dZoneList.hpp"
#include "list/3dZoneListTableModel.hpp"
#include "../../world/getworld.hpp"
#include "../../world/world.hpp"
#include "../../core/attributes.hpp"
#include "../../core/objectproperty.tpp"
#include "../../utils/displayname.hpp"

ThreeDZone::ThreeDZone(World& world, std::string_view _id)
  : IdObject(world, _id)
  , width{this, "width", 10.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , height{this, "height", 10.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speakerSetup{this, "speaker_setup", SpeakerSetup::Quadraphonic, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker0VolumeOverride{this, "speaker_0_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker0AudioDevice{this, "speaker_0_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker0AudioChannel{this, "speaker_0_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker1VolumeOverride{this, "speaker_1_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker1AudioDevice{this, "speaker_1_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker1AudioChannel{this, "speaker_1_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker2VolumeOverride{this, "speaker_2_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker2AudioDevice{this, "speaker_2_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker2AudioChannel{this, "speaker_2_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker3VolumeOverride{this, "speaker_3_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker3AudioDevice{this, "speaker_3_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker3AudioChannel{this, "speaker_3_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker4VolumeOverride{this, "speaker_4_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker4AudioDevice{this, "speaker_4_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker4AudioChannel{this, "speaker_4_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker5VolumeOverride{this, "speaker_5_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker5AudioDevice{this, "speaker_5_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker5AudioChannel{this, "speaker_5_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker6VolumeOverride{this, "speaker_6_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker6AudioDevice{this, "speaker_6_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker6AudioChannel{this, "speaker_6_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker7VolumeOverride{this, "speaker_7_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker7AudioDevice{this, "speaker_7_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker7AudioChannel{this, "speaker_7_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker8VolumeOverride{this, "speaker_8_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker8AudioDevice{this, "speaker_8_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker8AudioChannel{this, "speaker_8_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker9VolumeOverride{this, "speaker_9_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker9AudioDevice{this, "speaker_9_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker9AudioChannel{this, "speaker_9_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store}
{
  Attributes::addDisplayName(width, "Width");
  Attributes::addMinMax(width, 0.1, 1000.0);
  m_interfaceItems.add(width);
  
  Attributes::addDisplayName(height, "Height");
  Attributes::addMinMax(height, 0.1, 1000.0);
  m_interfaceItems.add(height);
  
  Attributes::addDisplayName(speakerSetup, "Speaker Setup");
  Attributes::addValues(speakerSetup, speakerSetupValues);
  m_interfaceItems.add(speakerSetup);
  
  // Add all speaker properties
  auto addSpeakerProps = [this](int idx, Property<double>& vol, Property<std::string>& dev, Property<int>& ch) {
    std::string base = "Speaker " + std::to_string(idx);
    Attributes::addDisplayName(vol, base + " Volume");
    Attributes::addMinMax(vol, 0.0, 2.0);
    m_interfaceItems.add(vol);
    
    Attributes::addDisplayName(dev, base + " Device");
    m_interfaceItems.add(dev);
    
    Attributes::addDisplayName(ch, base + " Channel");
    Attributes::addMinMax(ch, 0, 32);
    m_interfaceItems.add(ch);
  };
  
  addSpeakerProps(0, speaker0VolumeOverride, speaker0AudioDevice, speaker0AudioChannel);
  addSpeakerProps(1, speaker1VolumeOverride, speaker1AudioDevice, speaker1AudioChannel);
  addSpeakerProps(2, speaker2VolumeOverride, speaker2AudioDevice, speaker2AudioChannel);
  addSpeakerProps(3, speaker3VolumeOverride, speaker3AudioDevice, speaker3AudioChannel);
  addSpeakerProps(4, speaker4VolumeOverride, speaker4AudioDevice, speaker4AudioChannel);
  addSpeakerProps(5, speaker5VolumeOverride, speaker5AudioDevice, speaker5AudioChannel);
  addSpeakerProps(6, speaker6VolumeOverride, speaker6AudioDevice, speaker6AudioChannel);
  addSpeakerProps(7, speaker7VolumeOverride, speaker7AudioDevice, speaker7AudioChannel);
  addSpeakerProps(8, speaker8VolumeOverride, speaker8AudioDevice, speaker8AudioChannel);
  addSpeakerProps(9, speaker9VolumeOverride, speaker9AudioDevice, speaker9AudioChannel);
  
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
  
  const int speakerCount = static_cast<int>(speakerSetup.value());
  
  // Enable/disable speaker properties based on speaker setup
  Attributes::setEnabled(speaker0VolumeOverride, editable && speakerCount > 0);
  Attributes::setEnabled(speaker0AudioDevice, editable && speakerCount > 0);
  Attributes::setEnabled(speaker0AudioChannel, editable && speakerCount > 0);
  
  Attributes::setEnabled(speaker1VolumeOverride, editable && speakerCount > 1);
  Attributes::setEnabled(speaker1AudioDevice, editable && speakerCount > 1);
  Attributes::setEnabled(speaker1AudioChannel, editable && speakerCount > 1);
  
  Attributes::setEnabled(speaker2VolumeOverride, editable && speakerCount > 2);
  Attributes::setEnabled(speaker2AudioDevice, editable && speakerCount > 2);
  Attributes::setEnabled(speaker2AudioChannel, editable && speakerCount > 2);
  
  Attributes::setEnabled(speaker3VolumeOverride, editable && speakerCount > 3);
  Attributes::setEnabled(speaker3AudioDevice, editable && speakerCount > 3);
  Attributes::setEnabled(speaker3AudioChannel, editable && speakerCount > 3);
  
  Attributes::setEnabled(speaker4VolumeOverride, editable && speakerCount > 4);
  Attributes::setEnabled(speaker4AudioDevice, editable && speakerCount > 4);
  Attributes::setEnabled(speaker4AudioChannel, editable && speakerCount > 4);
  
  Attributes::setEnabled(speaker5VolumeOverride, editable && speakerCount > 5);
  Attributes::setEnabled(speaker5AudioDevice, editable && speakerCount > 5);
  Attributes::setEnabled(speaker5AudioChannel, editable && speakerCount > 5);
  
  Attributes::setEnabled(speaker6VolumeOverride, editable && speakerCount > 6);
  Attributes::setEnabled(speaker6AudioDevice, editable && speakerCount > 6);
  Attributes::setEnabled(speaker6AudioChannel, editable && speakerCount > 6);
  
  Attributes::setEnabled(speaker7VolumeOverride, editable && speakerCount > 7);
  Attributes::setEnabled(speaker7AudioDevice, editable && speakerCount > 7);
  Attributes::setEnabled(speaker7AudioChannel, editable && speakerCount > 7);
  
  Attributes::setEnabled(speaker8VolumeOverride, editable && speakerCount > 8);
  Attributes::setEnabled(speaker8AudioDevice, editable && speakerCount > 8);
  Attributes::setEnabled(speaker8AudioChannel, editable && speakerCount > 8);
  
  Attributes::setEnabled(speaker9VolumeOverride, editable && speakerCount > 9);
  Attributes::setEnabled(speaker9AudioDevice, editable && speakerCount > 9);
  Attributes::setEnabled(speaker9AudioChannel, editable && speakerCount > 9);
}

SpeakerConfiguration ThreeDZone::getSpeaker(int index) const
{
  SpeakerConfiguration config;
  
  switch(index)
  {
    case 0:
      config.volumeOverride = speaker0VolumeOverride.value();
      config.audioDevice = speaker0AudioDevice.value();
      config.audioChannel = speaker0AudioChannel.value();
      break;
    case 1:
      config.volumeOverride = speaker1VolumeOverride.value();
      config.audioDevice = speaker1AudioDevice.value();
      config.audioChannel = speaker1AudioChannel.value();
      break;
    case 2:
      config.volumeOverride = speaker2VolumeOverride.value();
      config.audioDevice = speaker2AudioDevice.value();
      config.audioChannel = speaker2AudioChannel.value();
      break;
    case 3:
      config.volumeOverride = speaker3VolumeOverride.value();
      config.audioDevice = speaker3AudioDevice.value();
      config.audioChannel = speaker3AudioChannel.value();
      break;
    case 4:
      config.volumeOverride = speaker4VolumeOverride.value();
      config.audioDevice = speaker4AudioDevice.value();
      config.audioChannel = speaker4AudioChannel.value();
      break;
    case 5:
      config.volumeOverride = speaker5VolumeOverride.value();
      config.audioDevice = speaker5AudioDevice.value();
      config.audioChannel = speaker5AudioChannel.value();
      break;
    case 6:
      config.volumeOverride = speaker6VolumeOverride.value();
      config.audioDevice = speaker6AudioDevice.value();
      config.audioChannel = speaker6AudioChannel.value();
      break;
    case 7:
      config.volumeOverride = speaker7VolumeOverride.value();
      config.audioDevice = speaker7AudioDevice.value();
      config.audioChannel = speaker7AudioChannel.value();
      break;
    case 8:
      config.volumeOverride = speaker8VolumeOverride.value();
      config.audioDevice = speaker8AudioDevice.value();
      config.audioChannel = speaker8AudioChannel.value();
      break;
    case 9:
      config.volumeOverride = speaker9VolumeOverride.value();
      config.audioDevice = speaker9AudioDevice.value();
      config.audioChannel = speaker9AudioChannel.value();
      break;
  }
  
  return config;
}
