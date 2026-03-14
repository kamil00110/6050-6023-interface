/**
 * server/src/hardware/camera/cameraenumerator.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */

#include "cameraenumerator.hpp"

#ifdef __linux__

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

std::vector<LocalCameraInfo> enumerateLocalCameras()
{
  std::vector<LocalCameraInfo> result;

  for(int i = 0; i < 16; i++)
  {
    const std::string path = "/dev/video" + std::to_string(i);
    const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if(fd < 0)
      continue;

    struct v4l2_capability cap{};
    if(::ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0 &&
       (cap.device_caps & V4L2_CAP_VIDEO_CAPTURE))
    {
      const std::string cardName = reinterpret_cast<const char*>(cap.card);
      result.push_back({std::to_string(i), cardName + " (" + path + ")"});
    }
    ::close(fd);
  }
  return result;
}

#else // Windows / macOS — no lightweight enumeration without platform SDKs

std::vector<LocalCameraInfo> enumerateLocalCameras()
{
  std::vector<LocalCameraInfo> result;
  for(int i = 0; i < 10; i++)
    result.push_back({std::to_string(i), "Camera " + std::to_string(i)});
  return result;
}

#endif
