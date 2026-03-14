/**
 * server/src/hardware/camera/cameraenumerator.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAMERAENUMERATOR_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAMERAENUMERATOR_HPP

#include <string>
#include <vector>

struct LocalCameraInfo
{
  std::string device; ///< index string ("0", "1", …) passed to OpenCV
  std::string name;   ///< human-readable label shown in the UI
};

/// Enumerate locally attached video-capture devices.
/// Linux  : queries V4L2 (VIDIOC_QUERYCAP) for indices 0-15.
/// Windows / macOS : offers indices 0-9 (no lightweight SDK-free enumeration).
std::vector<LocalCameraInfo> enumerateLocalCameras();

#endif
