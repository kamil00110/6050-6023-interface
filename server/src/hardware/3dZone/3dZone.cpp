#include "3dZone.hpp"
#include "list/3dZoneList.hpp"
#include "list/3dZoneListTableModel.hpp"
#include "../../world/getworld.hpp"
#include "../../world/world.hpp"
#include "../../world/worldsaver.hpp"
#include "../../world/worldloader.hpp"
#include "../../core/attributes.hpp"
#include "../../core/objectproperty.tpp"
#include "../../utils/displayname.hpp"

using nlohmann::json;

void SpeakerConfiguration::save(json& j) const
{
  j["volume_override"] = volumeOverride;
  j["audio_device"] = audioDevice;
  j["audio_channel"] = audioChannel;
}

void SpeakerConfiguration::load(const json& j)
{
  if(j.contains("volume_override"))
    volumeOverride = j["volume_override"];
  if(j.contains("audio_device"))
    audioDevice = j["audio_device"];
  if(j.contains("audio_channel"))
    audioChannel = j["audio_channel"];
}

ThreeDZone::ThreeDZone(World& world, std::string_view _id)
  : IdObject(world, _id)
  , width{this, "width", 10.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , height{this, "height", 10.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speakerSetup{this, "speaker_setup", SpeakerSetup::Quadraphonic, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speaker0VolumeOverride{this, "speaker_0_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value) { m_speakers[0].volumeOverride = value; }}
  , speaker0AudioDevice{this, "speaker_0_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& value) { m_speakers[0].audioDevice = value; }}
  , speaker0AudioChannel{this, "speaker_0_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](int value) { m_speakers[0].audioChannel = value; }}
  , speaker1VolumeOverride{this, "speaker_1_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value) { m_speakers[1].volumeOverride = value; }}
  , speaker1AudioDevice{this, "speaker_1_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& value) { m_speakers[1].audioDevice = value; }}
  , speaker1AudioChannel{this, "speaker_1_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](int value) { m_speakers[1].audioChannel = value; }}
  , speaker2VolumeOverride{this, "speaker_2_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value) { m_speakers[2].volumeOverride = value; }}
  , speaker2AudioDevice{this, "speaker_2_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& value) { m_speakers[2].audioDevice = value; }}
  , speaker2AudioChannel{this, "speaker_2_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](int value) { m_speakers[2].audioChannel = value; }}
  , speaker3VolumeOverride{this, "speaker_3_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value) { m_speakers[3].volumeOverride = value; }}
  , speaker3AudioDevice{this, "speaker_3_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& value) { m_speakers[3].audioDevice = value; }}
  , speaker3AudioChannel{this, "speaker_3_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](int value) { m_speakers[3].audioChannel = value; }}
  , speaker4VolumeOverride{this, "speaker_4_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value) { m_speakers[4].volumeOverride = value; }}
  , speaker4AudioDevice{this, "speaker_4_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& value) { m_speakers[4].audioDevice = value; }}
  , speaker4AudioChannel{this, "speaker_4_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](int value) { m_speakers[4].audioChannel = value; }}
  , speaker5VolumeOverride{this, "speaker_5_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value) { m_speakers[5].volumeOverride = value; }}
  , speaker5AudioDevice{this, "speaker_5_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& value) { m_speakers[5].audioDevice = value; }}
  , speaker5AudioChannel{this, "speaker_5_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](int value) { m_speakers[5].audioChannel = value; }}
  , speaker6VolumeOverride{this, "speaker_6_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value) { m_speakers[6].volumeOverride = value; }}
  , speaker6AudioDevice{this, "speaker_6_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& value) { m_speakers[6].audioDevice = value; }}
  , speaker6AudioChannel{this, "speaker_6_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](int value) { m_speakers[6].audioChannel = value; }}
  , speaker7VolumeOverride{this, "speaker_7_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value) { m_speakers[7].volumeOverride = value; }}
  , speaker7AudioDevice{this, "speaker_7_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& value) { m_speakers[7].audioDevice = value; }}
  , speaker7AudioChannel{this, "speaker_7_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](int value) { m_speakers[7].audioChannel = value; }}
  , speaker8VolumeOverride{this, "speaker_8_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value) { m_speakers[8].volumeOverride = value; }}
  , speaker8AudioDevice{this, "speaker_8_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& value) { m_speakers[8].audioDevice = value; }}
  , speaker8AudioChannel{this, "speaker_8_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](int value) { m_speakers[8].audioChannel = value; }}
  , speaker9VolumeOverride{this, "speaker_9_volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](double value) { m_speakers[9].volumeOverride = value; }}
  , speaker9AudioDevice{this, "speaker_9_audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& value) { m_speakers[9].audioDevice = value; }}
  , speaker9AudioChannel{this, "speaker_9_audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](int value) { m_speakers[9].audioChannel = value; }}
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

void ThreeDZone::save(WorldSaver& saver, json& data, json& state)
{
  IdObject::save(saver, data, state);
  
  json speakers = json::array();
  for(const auto& speaker : m_speakers)
  {
    json s;
    speaker.save(s);
    speakers.push_back(s);
  }
  data["speakers"] = speakers;
}

void ThreeDZone::load(WorldLoader& loader, const json& data)
{
  IdObject::load(loader, data);
  
  if(data.contains("speakers") && data["speakers"].is_array())
  {
    const auto& speakers = data["speakers"];
    for(size_t i = 0; i < std::min(speakers.size(), m_speakers.size()); i++)
    {
      m_speakers[i].load(speakers[i]);
    }
  }
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

void ThreeDZone::setSpeaker(int index, const SpeakerConfiguration& config)
{
  if(index >= 0 && index < 10)
  {
    m_speakers[index] = config;
  }
}
