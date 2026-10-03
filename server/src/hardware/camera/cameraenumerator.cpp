/**
 * server/src/hardware/camera/cameraenumerator.cpp
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
#include <algorithm>
#include <windows.h>
#include <dshow.h>
#include <mfapi.h>
#include <mfidl.h>
#include <combaseapi.h>

#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")

static void freeMediaType(AM_MEDIA_TYPE* pmt)
{
  if(!pmt)
    return;
  if(pmt->cbFormat != 0 && pmt->pbFormat)
    CoTaskMemFree(pmt->pbFormat);
  if(pmt->pUnk)
    pmt->pUnk->Release();
  CoTaskMemFree(pmt);
}

// Ask the device (via IAMStreamConfig) which capture resolutions it declares.
// Returns them largest-first, deduplicated. Not every declared size necessarily
// yields a usable (non-black) frame in OpenCV -- that is checked at capture time.
static std::vector<std::pair<uint32_t, uint32_t>> enumerateDirectShowResolutions(IMoniker* pMoniker)
{
  std::vector<std::pair<uint32_t, uint32_t>> result;

  IBaseFilter* pFilter = nullptr;
  if(FAILED(pMoniker->BindToObject(nullptr, nullptr, IID_IBaseFilter,
      reinterpret_cast<void**>(&pFilter))) || !pFilter)
    return result;

  IEnumPins* pEnumPins = nullptr;
  if(SUCCEEDED(pFilter->EnumPins(&pEnumPins)) && pEnumPins)
  {
    IPin* pPin = nullptr;
    while(pEnumPins->Next(1, &pPin, nullptr) == S_OK)
    {
      PIN_DIRECTION dir;
      if(SUCCEEDED(pPin->QueryDirection(&dir)) && dir == PINDIR_OUTPUT)
      {
        IAMStreamConfig* pConfig = nullptr;
        if(SUCCEEDED(pPin->QueryInterface(IID_IAMStreamConfig,
            reinterpret_cast<void**>(&pConfig))) && pConfig)
        {
          int count = 0, size = 0;
          if(SUCCEEDED(pConfig->GetNumberOfCapabilities(&count, &size)) &&
             size == sizeof(VIDEO_STREAM_CONFIG_CAPS))
          {
            for(int i = 0; i < count; ++i)
            {
              VIDEO_STREAM_CONFIG_CAPS caps{};
              AM_MEDIA_TYPE* pmt = nullptr;
              if(SUCCEEDED(pConfig->GetStreamCaps(i, &pmt,
                  reinterpret_cast<BYTE*>(&caps))) && pmt)
              {
                if(pmt->formattype == FORMAT_VideoInfo && pmt->pbFormat &&
                   pmt->cbFormat >= sizeof(VIDEOINFOHEADER))
                {
                  const auto* vih = reinterpret_cast<const VIDEOINFOHEADER*>(pmt->pbFormat);
                  const long w = vih->bmiHeader.biWidth;
                  const long h = vih->bmiHeader.biHeight;
                  const uint32_t uw = static_cast<uint32_t>(w < 0 ? -w : w);
                  const uint32_t uh = static_cast<uint32_t>(h < 0 ? -h : h);
                  if(uw != 0 && uh != 0)
                    result.emplace_back(uw, uh);
                }
                freeMediaType(pmt);
              }
            }
          }
          pConfig->Release();
        }
      }
      pPin->Release();
      if(!result.empty())
        break; // the first output pin with capabilities is enough
    }
    pEnumPins->Release();
  }
  pFilter->Release();

  std::sort(result.begin(), result.end(),
    [](const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b)
    {
      const uint64_t pa = static_cast<uint64_t>(a.first) * a.second;
      const uint64_t pb = static_cast<uint64_t>(b.first) * b.second;
      return (pa != pb) ? (pa > pb) : (a > b); // area desc, then a total-order tiebreak
    });
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

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

    auto resolutions = enumerateDirectShowResolutions(pMoniker);
    result.push_back({std::to_string(index), displayName, std::move(resolutions)});
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
  // CoInitializeEx returns S_FALSE when COM is already initialized on this
  // thread; that still counts as a successful init and needs a matching
  // CoUninitialize. Only call CoUninitialize when the init actually succeeded,
  // and exactly once.
  const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if(SUCCEEDED(hr))
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

  // COM unavailable, or nothing enumerated: offer a few bare indices.
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
