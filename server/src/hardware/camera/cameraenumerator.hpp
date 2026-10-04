/**
 * server/src/hardware/camera/cameraenumerator.hpp
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

#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAMERAENUMERATOR_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAMERAENUMERATOR_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct LocalCameraInfo
{
  std::string device; ///< index string ("0", "1", …) passed to OpenCV
  std::string name;   ///< human-readable label shown in the UI
  /// Resolutions the device reports it supports (width, height). May be empty
  /// when the platform can't enumerate them; the camera then falls back to its
  /// own default. Not every reported size necessarily yields a usable (non-black)
  /// frame -- that is checked at capture time. Default-initialized so the 2-field
  /// aggregate initializers (Linux / Media Foundation / fallbacks) stay valid
  /// under -Werror=missing-field-initializers.
  std::vector<std::pair<uint32_t, uint32_t>> resolutions{};
};

/// Enumerate locally attached video-capture devices, with the resolutions each
/// one declares (see LocalCameraInfo::resolutions).
/// Linux   : V4L2 -- VIDIOC_QUERYCAP for devices /dev/video0-15, plus
///           VIDIOC_ENUM_FRAMESIZES for the declared resolutions.
/// Windows : DirectShow (primary) + Media Foundation, with IAMStreamConfig for
///           the declared resolutions; falls back to bare indices if COM fails.
/// macOS   : AVFoundation -- AVCaptureDevice for the device names and declared
///           resolutions (implemented in cameraenumerator_mac.mm).
/// other   : a few bare indices (no SDK-free enumeration).
std::vector<LocalCameraInfo> enumerateLocalCameras();

#endif
