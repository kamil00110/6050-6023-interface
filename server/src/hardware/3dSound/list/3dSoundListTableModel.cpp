/**
 * server/src/hardware/3dSound/list/3dSoundListTableModel.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "3dSoundListTableModel.hpp"
#include "3dSoundList.hpp"
#include "../../../utils/displayname.hpp"
#include <sstream>
#include <iomanip>

bool ThreeDSoundListTableModel::isListedProperty(std::string_view name)
{
  return
    name == "id" ||
    name == "name" ||
    name == "position_x" ||
    name == "position_y" ||
    name == "position_z" ||
    name == "volume";
}

static std::string_view displayName(ThreeDSoundListColumn column)
{
  switch(column)
  {
    case ThreeDSoundListColumn::Id:
      return DisplayName::Object::id;

    case ThreeDSoundListColumn::Name:
      return DisplayName::Object::name;

    case ThreeDSoundListColumn::Position:
      return "Position";

    case ThreeDSoundListColumn::Volume:
      return "Volume";
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
        return sound.id;

      case ThreeDSoundListColumn::Name:
        return sound.name;

      case ThreeDSoundListColumn::Position:
      {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "(" << sound.positionX.value() 
            << ", " << sound.positionY.value() 
            << ", " << sound.positionZ.value() << ")";
        return oss.str();
      }

      case ThreeDSoundListColumn::Volume:
      {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0);
        oss << (sound.volume.value() * 100.0) << "%";
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

  if(name == "id")
    changed(row, ThreeDSoundListColumn::Id);
  else if(name == "name")
    changed(row, ThreeDSoundListColumn::Name);
  else if(name == "position_x" || name == "position_y" || name == "position_z")
    changed(row, ThreeDSoundListColumn::Position);
  else if(name == "volume")
    changed(row, ThreeDSoundListColumn::Volume);
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
