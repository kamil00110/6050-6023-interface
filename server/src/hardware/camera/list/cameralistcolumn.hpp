/**
 * server/src/hardware/camera/list/cameralistcolumn.hpp
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

#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_LIST_CAMERALISTCOLUMN_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_LIST_CAMERALISTCOLUMN_HPP

#include <type_traits>
#include <array>

enum class CameraListColumn
{
  Id      = 1 << 0,
  Name    = 1 << 1,
  Type    = 1 << 2,
  Device  = 1 << 3,
  Enabled = 1 << 4,
};

constexpr std::array<CameraListColumn, 5> cameraListColumnValues = {
  CameraListColumn::Id,
  CameraListColumn::Name,
  CameraListColumn::Type,
  CameraListColumn::Device,
  CameraListColumn::Enabled,
};

constexpr CameraListColumn operator|(CameraListColumn lhs, CameraListColumn rhs)
{
  return static_cast<CameraListColumn>(
    static_cast<std::underlying_type_t<CameraListColumn>>(lhs) |
    static_cast<std::underlying_type_t<CameraListColumn>>(rhs));
}

constexpr bool contains(CameraListColumn value, CameraListColumn mask)
{
  using T = std::underlying_type_t<CameraListColumn>;
  return (static_cast<T>(value) & static_cast<T>(mask)) == static_cast<T>(mask);
}

#endif
