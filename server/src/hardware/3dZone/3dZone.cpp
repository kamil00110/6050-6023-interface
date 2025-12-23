#include "3dZone.hpp"
#include "list/3dZoneList.hpp"
#include "list/3dZoneListTableModel.hpp"
#include "../../world/getworld.hpp"
#include "../../world/world.hpp"
#include "../../core/attributes.hpp"
#include "../../core/method.tpp"
#include "../../core/objectproperty.tpp"
#include "../../utils/displayname.hpp"
#include <nlohmann/json.hpp>

using nlohmann::json;

static std::string initializeSpeakers(SpeakerSetup setup, double width, double height)
{
  // ... existing code ...
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
  , openEditor{*this, "open_editor", MethodFlags::NoScript,  // Add this
      [this]()
      {
        // This will trigger the client to open the editor window
        // The client handles this via the object's interface
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
  Attributes::addVisible(speakersData, false);  // Hide the raw JSON from normal view
  m_interfaceItems.add(speakersData);
  
  // Add the editor button
  Attributes::addDisplayName(openEditor, "Open Editor");
  Attributes::addEnabled(openEditor, true);
  m_interfaceItems.add(openEditor);
  
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
