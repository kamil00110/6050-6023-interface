/**
 * shared/src/traintastic/enum/cameratype.hpp
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

#ifndef TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_CAMERATYPE_HPP
#define TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_CAMERATYPE_HPP

#include <cstdint>
#include <array>
#include <traintastic/enum/enum.hpp>

enum class CameraType : uint8_t
{
  Local = 0, //!< Local USB / V4L2 camera
  RTSP  = 1, //!< IP camera via RTSP
  MJPEG = 2, //!< IP camera via MJPEG-over-HTTP
};

TRAINTASTIC_ENUM(CameraType, "camera_type", 3,
{
  {CameraType::Local, "local"},
  {CameraType::RTSP,  "rtsp"},
  {CameraType::MJPEG, "mjpeg"},
});

constexpr std::array<CameraType, 3> cameraTypeValues{{
  CameraType::Local,
  CameraType::RTSP,
  CameraType::MJPEG,
}};

#endif
