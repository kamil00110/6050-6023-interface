/**
 * server/src/hardware/camera/cameratype.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAMERATYPE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAMERATYPE_HPP

#include <cstdint>
#include <array>
#include <string_view>

enum class CameraType : uint8_t
{
  Local = 0,
  RTSP  = 1,
  MJPEG = 2,
};

constexpr std::array<CameraType, 3> cameraTypeValues = {
  CameraType::Local,
  CameraType::RTSP,
  CameraType::MJPEG,
};

constexpr std::array<std::string_view, 3> cameraTypeNames = {
  "local",
  "rtsp",
  "mjpeg",
};

#endif
