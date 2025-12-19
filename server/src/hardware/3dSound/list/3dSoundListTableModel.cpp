#include "3dSoundListTableModel.hpp"
#include "3dSoundList.hpp"
#include "../../../utils/displayname.hpp"
#include <sstream>
#include <iomanip>

bool ThreeDSoundListTableModel::isListedProperty(std::string_view name)
{
    return
        name == "id" ||  // Id
        name == "sound_file" ||  // File
        name == "looping"   ||   // Loop
        name == "volume"    ||   // Volume
        name == "speed";         // Speed
}

static std::string_view displayName(ThreeDSoundListColumn column)
{
    switch(column)
    {
        case ThreeDSoundListColumn::Id:   return "Id";
        case ThreeDSoundListColumn::File:   return "File";
        case ThreeDSoundListColumn::Loop:   return "Loop";
        case ThreeDSoundListColumn::Volume: return "Volume";
        case ThreeDSoundListColumn::Speed:  return "Speed";
    }
    assert(false);
    return {};
}

ThreeDSoundListTableModel::ThreeDSoundListTableModel(ThreeDSoundList& list)
    : ObjectListTableModel<ThreeDSound>(list)
{
    std::vector<std::string_view> labels;

    for(auto column : threeDSoundListColumnValues)
    {
        if(contains(list.columns, column))
        {
            labels.emplace_back(displayName(column));
            m_columns.emplace_back(column);
        }
    }

    setColumnHeaders(std::move(labels));
}

std::string ThreeDSoundListTableModel::getText(uint32_t column, uint32_t row) const
{
    if(row < rowCount())
    {
        const ThreeDSound& sound = getItem(row);

        assert(column < m_columns.size());
        switch(m_columns[column])
        {
            case ThreeDSoundListColumn::Id:
                return sound.Id.value();
            
            case ThreeDSoundListColumn::File:
                return sound.soundFile.value();

            case ThreeDSoundListColumn::Loop:
                return sound.looping.value() ? "Yes" : "No";

            case ThreeDSoundListColumn::Volume:
            {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(0);
                oss << (sound.volume.value() * 100.0) << "%";
                return oss.str();
            }

            case ThreeDSoundListColumn::Speed:
            {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2);
                oss << sound.speed.value();
                return oss.str();
            }
        }
        assert(false);
    }

    return "";
}

void ThreeDSoundListTableModel::propertyChanged(BaseProperty& property, uint32_t row)
{
    std::string_view name = property.name();
    
    if(name == "id")      changed(row, ThreeDSoundListColumn::Id);
    else if(name == "sound_file")      changed(row, ThreeDSoundListColumn::File);
    else if(name == "looping")    changed(row, ThreeDSoundListColumn::Loop);
    else if(name == "volume")     changed(row, ThreeDSoundListColumn::Volume);
    else if(name == "speed")      changed(row, ThreeDSoundListColumn::Speed);
}

void ThreeDSoundListTableModel::changed(uint32_t row, ThreeDSoundListColumn column)
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
