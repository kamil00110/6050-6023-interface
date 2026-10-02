/**
 * shared/src/traintastic/enum/cameraresolution.hpp
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

#ifndef TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_CAMERARESOLUTION_HPP
#define TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_CAMERARESOLUTION_HPP

#include <cstdint>
#include <array>
#include <utility>
#include "enum.hpp"

// Requested capture resolution for local cameras. "Auto" leaves the camera at
// its own default; the capture requests the others from the device best-effort.
enum class CameraResolution : uint8_t
{
  Auto      = 0,
  R640x360  = 1,  // 360p  16:9
  R640x480  = 2,  // 480p   4:3
  R854x480  = 3,  // 480p  16:9
  R800x600  = 4,  // SVGA   4:3
  R1024x768 = 5,  // XGA    4:3
  R1280x720 = 6,  // 720p  16:9
  R1600x1200= 7,  // UXGA   4:3
  R1920x1080= 8,  // 1080p 16:9
  R2560x1440= 9,  // 1440p 16:9
  R3840x2160= 10, // 2160p 16:9 (4K)
};

TRAINTASTIC_ENUM(CameraResolution, "camera_resolution", 11,
{
  {CameraResolution::Auto,       "auto"},
  {CameraResolution::R640x360,   "640x360"},
  {CameraResolution::R640x480,   "640x480"},
  {CameraResolution::R854x480,   "854x480"},
  {CameraResolution::R800x600,   "800x600"},
  {CameraResolution::R1024x768,  "1024x768"},
  {CameraResolution::R1280x720,  "1280x720"},
  {CameraResolution::R1600x1200, "1600x1200"},
  {CameraResolution::R1920x1080, "1920x1080"},
  {CameraResolution::R2560x1440, "2560x1440"},
  {CameraResolution::R3840x2160, "3840x2160"},
});

constexpr std::array<CameraResolution, 11> cameraResolutionValues = {
  CameraResolution::Auto,
  CameraResolution::R640x360,
  CameraResolution::R640x480,
  CameraResolution::R854x480,
  CameraResolution::R800x600,
  CameraResolution::R1024x768,
  CameraResolution::R1280x720,
  CameraResolution::R1600x1200,
  CameraResolution::R1920x1080,
  CameraResolution::R2560x1440,
  CameraResolution::R3840x2160,
};

// Pixel size for a resolution; {0, 0} for Auto (do not request a size).
constexpr std::pair<uint32_t, uint32_t> toResolutionSize(CameraResolution r)
{
  switch(r)
  {
    case CameraResolution::R640x360:   return {640, 360};
    case CameraResolution::R640x480:   return {640, 480};
    case CameraResolution::R854x480:   return {854, 480};
    case CameraResolution::R800x600:   return {800, 600};
    case CameraResolution::R1024x768:  return {1024, 768};
    case CameraResolution::R1280x720:  return {1280, 720};
    case CameraResolution::R1600x1200: return {1600, 1200};
    case CameraResolution::R1920x1080: return {1920, 1080};
    case CameraResolution::R2560x1440: return {2560, 1440};
    case CameraResolution::R3840x2160: return {3840, 2160};
    case CameraResolution::Auto:       break;
  }
  return {0, 0};
}

#endif
