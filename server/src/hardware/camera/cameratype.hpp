/**
 * shared/src/traintastic/enum/cameratype.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */
#ifndef TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_CAMERATYPE_HPP
#define TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_CAMERATYPE_HPP

#include <cstdint>
#include <array>

enum class CameraType : uint8_t
{
  Local = 0,
  RTSP  = 1,
  MJPEG = 2,
  RTMP  = 3,
  HLS   = 4,
};

constexpr std::array<CameraType, 5> cameraTypeValues = {
  CameraType::Local,
  CameraType::RTSP,
  CameraType::MJPEG,
  CameraType::RTMP,
  CameraType::HLS,
};

#endif
