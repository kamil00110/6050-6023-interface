#include "3dZoneListTableModel.hpp"
#include "3dZoneList.hpp"
#include "../../../utils/displayname.hpp"
#include <sstream>
#include <iomanip>

bool ThreeDZoneListTableModel::isListedProperty(std::string_view name)
{
  return
    name == "id" ||
    name == "width" ||
    name == "height" ||
    name == "speaker_setup" ||
    name.find("speaker_") == 0;  // Any speaker property
}

static std::string_view displayName(ThreeDZoneListColumn column)
{
  switch(column)
  {
    case ThreeDZoneListColumn::Id: return "Id";
    case ThreeDZoneListColumn::Width: return "Width";
    case ThreeDZoneListColumn::Height: return "Height";
    case ThreeDZoneListColumn::SpeakerSetup: return "Setup";
    case ThreeDZoneListColumn::Speakers: return "Speakers";
  }
  assert(false);
  return {};
}

static std::string speakerSetupToString(SpeakerSetup setup)
{
  switch(setup)
  {
    case SpeakerSetup::Quadraphonic: return "4.0";
    case SpeakerSetup::Surround5_1: return "5.1";
    case SpeakerSetup::Surround7_1: return "7.1";
    case SpeakerSetup::Surround9_1: return "9.1";
  }
  return "Unknown";
}

static std::string formatSpeaker(const SpeakerConfiguration& speaker, int index)
{
  std::ostringstream oss;
  oss << "[" << index << "] ";
  oss << "Vol:" << std::fixed << std::setprecision(0) << (speaker.volumeOverride * 100) << "%";
  if(!speaker.audioDevice.empty())
    oss << " Dev:" << speaker.audioDevice;
  oss << " Ch:" << speaker.audioChannel;
  return oss.str();
}

static std::string formatAllSpeakers(const ThreeDZone& zone)
{
  const int speakerCount = static_cast<int>(zone.speakerSetup.value());
  std::ostringstream oss;
  
  for(int i = 0; i < speakerCount; i++)
  {
    if(i > 0)
      oss << " | ";
    oss << formatSpeaker(zone.getSpeaker(i), i);
  }
  
  return oss.str();
}

ThreeDZoneListTableModel::ThreeDZoneListTableModel(ThreeDZoneList& list)
  : ObjectListTableModel<ThreeDZone>(list)
{
  std::vector<std::string_view> labels;
  for(auto column : threeDZoneListColumnValues)
  {
    if(contains(list.columns, column))
    {
      labels.emplace_back(displayName(column));
      m_columns.emplace_back(column);
    }
  }
  setColumnHeaders(std::move(labels));
}

std::string ThreeDZoneListTableModel::getText(uint32_t column, uint32_t row) const
{
  if(row < rowCount())
  {
    const ThreeDZone& zone = getItem(row);
    assert(column < m_columns.size());
    
    switch(m_columns[column])
    {
      case ThreeDZoneListColumn::Id:
        return zone.id.value();
        
      case ThreeDZoneListColumn::Width:
      {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << zone.width.value() << "m";
        return oss.str();
      }
      
      case ThreeDZoneListColumn::Height:
      {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << zone.height.value() << "m";
        return oss.str();
      }
      
      case ThreeDZoneListColumn::SpeakerSetup:
        return speakerSetupToString(zone.speakerSetup.value());
        
      case ThreeDZoneListColumn::Speakers:
        return formatAllSpeakers(zone);
    }
    assert(false);
  }
  return "";
}

void ThreeDZoneListTableModel::propertyChanged(BaseProperty& property, uint32_t row)
{
  std::string_view name = property.name();
  
  if(name == "id") 
    changed(row, ThreeDZoneListColumn::Id);
  else if(name == "width") 
    changed(row, ThreeDZoneListColumn::Width);
  else if(name == "height") 
    changed(row, ThreeDZoneListColumn::Height);
  else if(name == "speaker_setup") 
  {
    changed(row, ThreeDZoneListColumn::SpeakerSetup);
    changed(row, ThreeDZoneListColumn::Speakers);  // Update speakers column when setup changes
  }
  else if(name.find("speaker_") == 0)
  {
    // Any speaker property change updates the Speakers column
    changed(row, ThreeDZoneListColumn::Speakers);
  }
}

void ThreeDZoneListTableModel::changed(uint32_t row, ThreeDZoneListColumn column)
{
  for(size_t i = 0; i < m_columns.size(); i++)
  {
    if(m_columns[i] == column)
    {
      TableModel::changed(row, static_cast<uint32_t>(i));
      return;
    }
  }
}
