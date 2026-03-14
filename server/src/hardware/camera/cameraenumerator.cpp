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
      // cap.card may be empty on some systems — fall back to the device path
      const std::string cardName = reinterpret_cast<const char*>(cap.card);
      const std::string displayName = cardName.empty()
        ? path
        : cardName + " (" + path + ")";

      result.push_back({std::to_string(i), displayName});
    }
    ::close(fd);
  }

  // If V4L2 found nothing (e.g. no cameras attached), offer index 0 as a
  // generic fallback so the user can still try to open a camera.
  if(result.empty())
    result.push_back({"0", "/dev/video0"});

  return result;
}

#elif defined(_WIN32)

// Windows: enumerate via simple index probing — no extra SDK needed.
// OpenCV will try to open each index; we just offer them as options.
std::vector<LocalCameraInfo> enumerateLocalCameras()
{
  std::vector<LocalCameraInfo> result;
  for(int i = 0; i < 10; i++)
    result.push_back({std::to_string(i), "Camera " + std::to_string(i)});
  return result;
}

#else // macOS and other POSIX

std::vector<LocalCameraInfo> enumerateLocalCameras()
{
  std::vector<LocalCameraInfo> result;
  for(int i = 0; i < 10; i++)
    result.push_back({std::to_string(i), "Camera " + std::to_string(i)});
  return result;
}

#endif
