/**
 * server/src/hardware/camera/cameraenumerator.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */

#include "cameraenumerator.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Linux — V4L2
// ─────────────────────────────────────────────────────────────────────────────
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
      const std::string displayName = cardName.empty()
        ? path
        : cardName + " (" + path + ")";
      result.push_back({std::to_string(i), displayName});
    }
    ::close(fd);
  }

  if(result.empty())
    result.push_back({"0", "/dev/video0"});

  return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Windows — Media Foundation
// ─────────────────────────────────────────────────────────────────────────────
#elif defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <combaseapi.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")

std::vector<LocalCameraInfo> enumerateLocalCameras()
{
  std::vector<LocalCameraInfo> result;

  if(FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    goto fallback;

  if(FAILED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET)))
  {
    CoUninitialize();
    goto fallback;
  }

  {
    IMFAttributes* pConfig = nullptr;
    if(SUCCEEDED(MFCreateAttributes(&pConfig, 1)))
    {
      pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                       MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

      IMFActivate** ppDevices = nullptr;
      UINT32 count = 0;
      if(SUCCEEDED(MFEnumDeviceSources(pConfig, &ppDevices, &count)))  // fixed name
      {
        for(UINT32 i = 0; i < count; i++)
        {
          WCHAR* szFriendlyName = nullptr;
          UINT32 cchName = 0;
          std::string displayName = "Camera " + std::to_string(i);

          if(SUCCEEDED(ppDevices[i]->GetAllocatedString(
                MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                &szFriendlyName, &cchName)) && szFriendlyName)
          {
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0,
              szFriendlyName, -1, nullptr, 0, nullptr, nullptr);
            if(utf8Len > 0)
            {
              std::string utf8(utf8Len - 1, '\0');
              WideCharToMultiByte(CP_UTF8, 0, szFriendlyName, -1,
                utf8.data(), utf8Len, nullptr, nullptr);
              displayName = utf8;
            }
            CoTaskMemFree(szFriendlyName);
          }

          result.push_back({std::to_string(i), displayName});
          ppDevices[i]->Release();
        }
        CoTaskMemFree(ppDevices);
      }
      pConfig->Release();
    }
  }

  MFShutdown();
  CoUninitialize();

  if(!result.empty())
    return result;

fallback:
  result.clear();
  for(int i = 0; i < 4; i++)
    result.push_back({std::to_string(i), "Camera " + std::to_string(i)});
  return result;
}
// ─────────────────────────────────────────────────────────────────────────────
// macOS and other POSIX — index probing fallback
// ─────────────────────────────────────────────────────────────────────────────
#else

std::vector<LocalCameraInfo> enumerateLocalCameras()
{
  std::vector<LocalCameraInfo> result;
  for(int i = 0; i < 4; i++)
    result.push_back({std::to_string(i), "Camera " + std::to_string(i)});
  return result;
}

#endif
