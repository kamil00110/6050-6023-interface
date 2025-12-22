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
    case ThreeDZoneListColumn::Speaker0: return "Spkr 0";
    case ThreeDZoneListColumn::Speaker1: return "Spkr 1";
    case ThreeDZoneListColumn::Speaker2: return "Spkr 2";
    case ThreeDZoneListColumn::Speaker3: return "Spkr 3";
    case ThreeDZoneListColumn::Speaker4: return "Spkr 4";
    case ThreeDZoneListColumn::Speaker5: return "Spkr 5";
    case ThreeDZoneListColumn::Speaker6: return "Spkr 6";
    case ThreeDZoneListColumn::Speaker7: return "Spkr 7";
    case ThreeDZoneListColumn::Speaker8: return "Spkr 8";
    case ThreeDZoneListColumn::Speaker9: return "Spkr 9";
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

static std::string formatSpeaker(const SpeakerConfiguration& speaker)
{
  std::ostringstream oss;
  oss << "Vol:" << std::fixed << std::setprecision(1) << (speaker.volumeOverride * 100) << "%";
  if(!speaker.audioDevice.empty())
    oss << " Dev:" << speaker.audioDevice;
  oss << " Ch:" << speaker.audioChannel;
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
        
      case ThreeDZoneListColumn::Speaker0:
        return formatSpeaker(zone.getSpeaker(0));
      case ThreeDZoneListColumn::Speaker1:
        return formatSpeaker(zone.getSpeaker(1));
      case ThreeDZoneListColumn::Speaker2:
        return formatSpeaker(zone.getSpeaker(2));
      case ThreeDZoneListColumn::Speaker3:
        return formatSpeaker(zone.getSpeaker(3));
      case ThreeDZoneListColumn::Speaker4:
        return formatSpeaker(zone.getSpeaker(4));
      case ThreeDZoneListColumn::Speaker5:
        return formatSpeaker(zone.getSpeaker(5));
      case ThreeDZoneListColumn::Speaker6:
        return formatSpeaker(zone.getSpeaker(6));
      case ThreeDZoneListColumn::Speaker7:
        return formatSpeaker(zone.getSpeaker(7));
      case ThreeDZoneListColumn::Speaker8:
        return formatSpeaker(zone.getSpeaker(8));
      case ThreeDZoneListColumn::Speaker9:
        return formatSpeaker(zone.getSpeaker(9));
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
    changed(row, ThreeDZoneListColumn::SpeakerSetup);
  else if(name.find("speaker_0_") == 0)
    changed(row, ThreeDZoneListColumn::Speaker0);
  else if(name.find("speaker_1_") == 0)
    changed(row, ThreeDZoneListColumn::Speaker1);
  else if(name.find("speaker_2_") == 0)
    changed(row, ThreeDZoneListColumn::Speaker2);
  else if(name.find("speaker_3_") == 0)
    changed(row, ThreeDZoneListColumn::Speaker3);
  else if(name.find("speaker_4_") == 0)
    changed(row, ThreeDZoneListColumn::Speaker4);
  else if(name.find("speaker_5_") == 0)
    changed(row, ThreeDZoneListColumn::Speaker5);
  else if(name.find("speaker_6_") == 0)
    changed(row, ThreeDZoneListColumn::Speaker6);
  else if(name.find("speaker_7_") == 0)
    changed(row, ThreeDZoneListColumn::Speaker7);
  else if(name.find("speaker_8_") == 0)
    changed(row, ThreeDZoneListColumn::Speaker8);
  else if(name.find("speaker_
