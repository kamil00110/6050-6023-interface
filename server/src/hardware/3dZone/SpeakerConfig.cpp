#include "SpeakerConfig.hpp"
#include "3dZone.hpp"
#include "../../world/getworld.hpp"
#include "../../core/attributes.hpp"
#include "../../utils/displayname.hpp"

SpeakerConfig::SpeakerConfig(ThreeDZone& parent, int speakerIndex)
  : SubObject(parent, "speakers")  // FIX: SubObject needs parent property name
  , m_speakerIndex{speakerIndex}
  , volumeOverride{this, "volume_override", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , audioDevice{this, "audio_device", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , audioChannel{this, "audio_channel", 0, PropertyFlags::ReadWrite | PropertyFlags::Store}
{
  Attributes::addDisplayName(volumeOverride, "Volume Override");
  Attributes::addMinMax(volumeOverride, 0.0, 2.0);
  Attributes::addEnabled(volumeOverride, true);
  m_interfaceItems.add(volumeOverride);
  
  Attributes::addDisplayName(audioDevice, "Audio Device");
  Attributes::addEnabled(audioDevice, true);
  m_interfaceItems.add(audioDevice);
  
  Attributes::addDisplayName(audioChannel, "Audio Channel");
  Attributes::addMinMax(audioChannel, 0, 32);
  Attributes::addEnabled(audioChannel, true);
  m_interfaceItems.add(audioChannel);
  
  updateEnabled();
}

void SpeakerConfig::worldEvent(WorldState state, WorldEvent event)
{
  SubObject::worldEvent(state, event);
  updateEnabled();
}

void SpeakerConfig::updateEnabled()
{
  const bool editable = contains(getWorld(this->parent()).state.value(), WorldState::Edit);
  Attributes::setEnabled(volumeOverride, editable);
  Attributes::setEnabled(audioDevice, editable);
  Attributes::setEnabled(audioChannel, editable);
}
