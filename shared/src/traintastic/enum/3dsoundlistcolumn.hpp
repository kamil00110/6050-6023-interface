/**
 * shared/traintastic/enum/3dsoundlistcolumn.hpp
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

#ifndef TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_3DSOUNDLISTCOLUMN_HPP
#define TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_3DSOUNDLISTCOLUMN_HPP

#include <cstdint>
#include <array>
#include "../set/set.hpp"

enum class ThreeDSoundListColumn : uint8_t
{
  Id = 0,
  Name = 1,
  Position = 2,
  Volume = 3,
};

TRAINTASTIC_ENUM(ThreeDSoundListColumn, "3d_sound_list_column", 4,
{
  {ThreeDSoundListColumn::Id, "id"},
  {ThreeDSoundListColumn::Name, "name"},
  {ThreeDSoundListColumn::Position, "position"},
  {ThreeDSoundListColumn::Volume, "volume"},
});

constexpr auto threeDSoundListColumnValues = std::array<ThreeDSoundListColumn, 4>{
  ThreeDSoundListColumn::Id,
  ThreeDSoundListColumn::Name,
  ThreeDSoundListColumn::Position,
  ThreeDSoundListColumn::Volume,
};

using ThreeDSoundListColumnSet = Set<ThreeDSoundListColumn>;

#endif
