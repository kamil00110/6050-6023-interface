/**
 * shared/src/traintastic/enum/3dsoundlistcolumn.hpp
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

enum class ThreeDSoundListColumn : uint8_t
{
  Id = 1,
  File = 2,
  Loop = 4,
  Volume = 8,
  Speed = 16,
};

TRAINTASTIC_ENUM(ThreeDSoundListColumn, "3d_sound_list_column", 5,
{
  {ThreeDSoundListColumn::Id, "id"},
  {ThreeDSoundListColumn::File, "file"},
  {ThreeDSoundListColumn::Loop, "loop"},
  {ThreeDSoundListColumn::Volume, "volume"},
  {ThreeDSoundListColumn::Speed, "speed"},
});

constexpr auto threeDSoundListColumnValues = std::array<ThreeDSoundListColumn, 5>{
  ThreeDSoundListColumn::Id,
  ThreeDSoundListColumn::File,
  ThreeDSoundListColumn::Loop,
  ThreeDSoundListColumn::Volume,
  ThreeDSoundListColumn::Speed,
};

constexpr ThreeDSoundListColumn operator|(ThreeDSoundListColumn lhs, ThreeDSoundListColumn rhs)
{
  return static_cast<ThreeDSoundListColumn>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

constexpr ThreeDSoundListColumn operator&(ThreeDSoundListColumn lhs, ThreeDSoundListColumn rhs)
{
  return static_cast<ThreeDSoundListColumn>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

constexpr ThreeDSoundListColumn operator~(ThreeDSoundListColumn value)
{
  return static_cast<ThreeDSoundListColumn>(~static_cast<uint8_t>(value));
}

constexpr bool contains(ThreeDSoundListColumn value, ThreeDSoundListColumn mask)
{
  return (static_cast<uint8_t>(value) & static_cast<uint8_t>(mask)) == static_cast<uint8_t>(mask);
}

#endif
