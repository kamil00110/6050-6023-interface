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
#include "../../../shared/src/traintastic/enum/enum.hpp"

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

template<>
struct EnumName<CameraType>
{
  static constexpr std::string_view value = "camera_type";
};

template<>
struct EnumValues<CameraType>
{
  static constexpr std::array<std::pair<std::string_view, CameraType>, 5> value = {{
    {"local", CameraType::Local},
    {"rtsp",  CameraType::RTSP},
    {"mjpeg", CameraType::MJPEG},
    {"rtmp",  CameraType::RTMP},
    {"hls",   CameraType::HLS},
  }};
};

#endif
