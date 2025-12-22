#include "3dZone.hpp"
#include "list/3dZoneList.hpp"
#include "list/3dZoneListTableModel.hpp"
#include "../../world/getworld.hpp"
#include "../../world/world.hpp"
#include "../../core/attributes.hpp"
#include "../../core/objectproperty.tpp"
#include "../../utils/displayname.hpp"
#include <sstream> 

using nlohmann::json;

using nlohmann::json;

ThreeDZone::ThreeDZone(World& world, std::string_view _id)
  : IdObject(world, _id)
  , width{this, "width", 10.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , height{this, "height", 10.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speakerSetup{this, "speaker_setup", SpeakerSetup::Quadraphonic, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speakersData{this, "speakers_data", "[]", PropertyFlags::ReadWrite | PropertyFlags::Store}
{
  Attributes::addDisplayName(width, "Width (m)");
  Attributes::addMinMax(width, 0.1, 1000.0);
  m_interfaceItems.add(width);
  
  Attributes::addDisplayName(height, "Height (m)");
  Attributes::addMinMax(height, 0.1, 1000.0);
  m_interfaceItems.add(height);
  
  Attributes::addDisplayName(speakerSetup, "Speaker Setup");
  Attributes::addValues(speakerSetup, speakerSetupValues);
  m_interfaceItems.add(speakerSetup);
  
  Attributes::addDisplayName(speakersData, "Speakers Configuration");
  m_interfaceItems.add(speakersData);
  
  // Initialize default speaker data based on setup
  const int speakerCount = static_cast<int>(speakerSetup.value());
  json speakers = json::array();
  for(int i = 0; i < speakerCount; i++)
  {
    speakers.push_back({
      {"index", i},
      {"volume", 1.0},
      {"device", ""},
      {"channel", 0}
    });
  }
  speakersData.setValueInternal(speakers.dump());
  
  updateEnabled();
}

void ThreeDZone::addToWorld()
{
  IdObject::addToWorld();
  if(auto list = getWorld(*this).threeDZones.value())
    list->addObject(std::static_pointer_cast<ThreeDZone>(Object::shared_from_this()));
}

void ThreeDZone::loaded()
{
  IdObject::loaded();
  updateEnabled();
}

void ThreeDZone::destroying()
{
  if(auto list = getWorld(*this).threeDZones.value())
    list->removeObject(std::static_pointer_cast<ThreeDZone>(Object::shared_from_this()));
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

nlohmann::json ThreeDZone::getSpeakersJson() const
{
  try
  {
    return json::parse(speakersData.value());
  }
  catch(...)
  {
    return json::array();
  }
}

std::string ThreeDZone::getSpeakersFormatted() const
{
  try
  {
    const std::string dataStr = speakersData.value();
    if(dataStr.empty() || dataStr == "[]")
      return "";
    
    json speakers = json::parse(dataStr);
    if(!speakers.is_array() || speakers.empty())
      return "";
    
    const int speakerCount = static_cast<int>(speakerSetup.value());
    std::ostringstream oss;
    
    for(int i = 0; i < std::min(speakerCount, static_cast<int>(speakers.size())); i++)
    {
      if(i > 0)
        oss << " | ";
      
      const auto& speaker = speakers[i];
      oss << "[" << i << "] ";
      
      if(speaker.contains("volume") && speaker["volume"].is_number())
      {
        double vol = speaker["volume"].get<double>();
        oss << "Vol:" << std::fixed << std::setprecision(0) << (vol * 100) << "%";
      }
      else
      {
        oss << "Vol:100%";
      }
      
      if(speaker.contains("device") && speaker["device"].is_string())
      {
        std::string dev = speaker["device"].get<std::string>();
        if(!dev.empty())
          oss << " Dev:" << dev;
      }
      
      if(speaker.contains("channel") && speaker["channel"].is_number())
      {
        oss << " Ch:" << speaker["channel"].get<int>();
      }
      else
      {
        oss << " Ch:0";
      }
    }
    
    return oss.str();
  }
  catch(const std::exception& e)
  {
    return std::string("Parse error: ") + e.what();
  }
  catch(...)
  {
    return "Unknown error";
  }
}
