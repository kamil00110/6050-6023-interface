#include "3dZone.hpp"
#include "SpeakerConfig.hpp"
#include "list/3dZoneList.hpp"
#include "list/3dZoneListTableModel.hpp"  // ADD THIS - needed for complete type
#include "../../world/getworld.hpp"
#include "../../world/world.hpp"
#include "../../core/attributes.hpp"
#include "../../core/objectvectorproperty.tpp"
#include "../../utils/displayname.hpp"

ThreeDZone::ThreeDZone(World& world, std::string_view _id)
  : IdObject(world, _id)
  , width{this, "width", 10.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , height{this, "height", 10.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speakerSetup{this, "speaker_setup", SpeakerSetup::Quadraphonic, 
      PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](SpeakerSetup /*value*/)
      {
        updateSpeakerCount();
      }}
  , speakers{*this, "speakers", {}, PropertyFlags::ReadOnly | PropertyFlags::Store | PropertyFlags::SubObject}  // FIX: Use *this instead of this
{
  Attributes::addDisplayName(width, "Width");
  Attributes::addMinMax(width, 0.1, 1000.0);
  Attributes::addEnabled(width, true);
  m_interfaceItems.add(width);
  
  Attributes::addDisplayName(height, "Height");
  Attributes::addMinMax(height, 0.1, 1000.0);
  Attributes::addEnabled(height, true);
  m_interfaceItems.add(height);
  
  Attributes::addDisplayName(speakerSetup, "Speaker Setup");
  Attributes::addValues(speakerSetup, speakerSetupValues);  // FIX: lowercase 'speakerSetupValues'
  Attributes::addEnabled(speakerSetup, true);
  m_interfaceItems.add(speakerSetup);
  
  Attributes::addObjectEditor(speakers, false);
  m_interfaceItems.add(speakers);
  
  updateSpeakerCount();
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
}

void ThreeDZone::updateSpeakerCount()
{
  const int requiredCount = static_cast<int>(speakerSetup.value());
  const int currentCount = speakers.size();
  
  if(currentCount < requiredCount)
  {
    // Add speakers
    for(int i = currentCount; i < requiredCount; i++)
    {
      auto speaker = std::make_shared<SpeakerConfig>(*this, i);
      speakers.appendInternal(speaker);
    }
  }
  else if(currentCount > requiredCount)
  {
    // Remove excess speakers - FIX: use eraseInternal with index
    while(speakers.size() > static_cast<size_t>(requiredCount))
    {
      speakers.eraseInternal(speakers.size() - 1);
    }
  }
}
