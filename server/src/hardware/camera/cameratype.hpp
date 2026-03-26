

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
#include <traintastic/enum/enum.hpp>

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

TRAINTASTIC_ENUM(CameraType, "camera_type", 5,
{
  {CameraType::Local, "local"},
  {CameraType::RTSP,  "rtsp"},
  {CameraType::MJPEG, "mjpeg"},
  {CameraType::RTMP,  "rtmp"},
  {CameraType::HLS,   "hls"},
});

#endif
