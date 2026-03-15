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
#include <dshow.h>
#include <mfapi.h>
#include <mfidl.h>
#include <combaseapi.h>

#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")

static std::vector<LocalCameraInfo> enumerateViaDirectShow()
{
  std::vector<LocalCameraInfo> result;

  ICreateDevEnum* pDevEnum = nullptr;
  if(FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr,
      CLSCTX_INPROC_SERVER, IID_ICreateDevEnum,
      reinterpret_cast<void**>(&pDevEnum))))
    return result;

  IEnumMoniker* pEnum = nullptr;
  HRESULT hr = pDevEnum->CreateClassEnumerator(
      CLSID_VideoInputDeviceCategory, &pEnum, 0);
  pDevEnum->Release();

  if(hr != S_OK || !pEnum)  // S_FALSE means empty category
    return result;

  IMoniker* pMoniker = nullptr;
  int index = 0;
  while(pEnum->Next(1, &pMoniker, nullptr) == S_OK)
  {
    std::string displayName = "Camera " + std::to_string(index);

    IPropertyBag* pPropBag = nullptr;
    if(SUCCEEDED(pMoniker->BindToStorage(nullptr, nullptr,
        IID_IPropertyBag, reinterpret_cast<void**>(&pPropBag))))
    {
      VARIANT var{};
      VariantInit(&var);
      if(SUCCEEDED(pPropBag->Read(L"FriendlyName", &var, nullptr))
         && var.vt == VT_BSTR && var.bstrVal)
      {
        int len = WideCharToMultiByte(CP_UTF8, 0,
            var.bstrVal, -1, nullptr, 0, nullptr, nullptr);
        if(len > 0)
        {
          std::string utf8(len - 1, '\0');
          WideCharToMultiByte(CP_UTF8, 0,
              var.bstrVal, -1, utf8.data(), len, nullptr, nullptr);
          displayName = utf8;
        }
      }
      VariantClear(&var);
      pPropBag->Release();
    }

    result.push_back({std::to_string(index), displayName});
    pMoniker->Release();
    ++index;
  }
  pEnum->Release();
  return result;
}

static std::vector<LocalCameraInfo> enumerateViaMediaFoundation()
{
  std::vector<LocalCameraInfo> result;

  if(FAILED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET)))
    return result;

  IMFAttributes* pConfig = nullptr;
  if(SUCCEEDED(MFCreateAttributes(&pConfig, 1)))
  {
    pConfig->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                     MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;
    if(SUCCEEDED(MFEnumDeviceSources(pConfig, &ppDevices, &count)))
    {
      for(UINT32 i = 0; i < count; i++)
      {
        std::string displayName = "Camera " + std::to_string(i);
        WCHAR* szName = nullptr;
        UINT32 cch = 0;
        if(SUCCEEDED(ppDevices[i]->GetAllocatedString(
              MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &szName, &cch))
           && szName)
        {
          int len = WideCharToMultiByte(CP_UTF8, 0,
              szName, -1, nullptr, 0, nullptr, nullptr);
          if(len > 0)
          {
            std::string utf8(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0,
                szName, -1, utf8.data(), len, nullptr, nullptr);
            displayName = utf8;
          }
          CoTaskMemFree(szName);
        }
        result.push_back({std::to_string(i), displayName});
        ppDevices[i]->Release();
      }
      CoTaskMemFree(ppDevices);
    }
    pConfig->Release();
  }
  MFShutdown();
  return result;
}

std::vector<LocalCameraInfo> enumerateLocalCameras()
{
  if(FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
    goto fallback;

  {
    // DirectShow is the authoritative source — it sees hardware cameras,
    // OBS Virtual Camera, NVIDIA Broadcast, and every other WDM driver.
    // Media Foundation misses pure DirectShow virtual cameras entirely.
    // Strategy: use DirectShow as primary; if it finds nothing fall back
    // to MF; merge both sets deduplicating by friendly name.

    auto dsResult  = enumerateViaDirectShow();
    auto mfResult  = enumerateViaMediaFoundation();

    // Merge: add MF entries whose name isn't already in the DS list.
    // Both enumerators use a sequential integer index as the device key,
    // so the DS index is authoritative for OpenCV (CAP_DSHOW uses it).
    for(auto& mf : mfResult)
    {
      bool found = false;
      for(auto& ds : dsResult)
        if(ds.name == mf.name) { found = true; break; }
      if(!found)
        dsResult.push_back(std::move(mf));
    }

    CoUninitialize();

    if(!dsResult.empty())
      return dsResult;
  }

fallback:
  CoUninitialize();
  std::vector<LocalCameraInfo> result;
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
