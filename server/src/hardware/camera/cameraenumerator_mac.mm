/**
 * server/src/hardware/camera/cameraenumerator_mac.mm
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

// macOS implementation of enumerateLocalCameras(): queries AVFoundation for the
// attached video-capture devices, their human-readable names, and the capture
// resolutions each one declares. Built only on Apple platforms (see CMakeLists;
// the other platforms live in cameraenumerator.cpp).
#ifdef __APPLE__

#include "cameraenumerator.hpp"

#include <algorithm>

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>

// Ask AVFoundation which capture resolutions a device declares, across all of its
// formats. Returns them largest-first, deduplicated. Not every declared size
// necessarily yields a usable (non-black) frame in OpenCV -- that is handled at
// capture time.
static std::vector<std::pair<uint32_t, uint32_t>> enumerateAVFoundationResolutions(AVCaptureDevice* device)
{
  std::vector<std::pair<uint32_t, uint32_t>> result;

  for(AVCaptureDeviceFormat* format in device.formats)
  {
    CMFormatDescriptionRef desc = format.formatDescription;
    if(!desc)
      continue;
    const CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(desc);
    if(dim.width > 0 && dim.height > 0)
      result.emplace_back(static_cast<uint32_t>(dim.width), static_cast<uint32_t>(dim.height));
  }

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

std::vector<LocalCameraInfo> enumerateLocalCameras()
{
  std::vector<LocalCameraInfo> result;

  @autoreleasepool
  {
    // +devicesWithMediaType: is deprecated in favour of AVCaptureDeviceDiscoverySession,
    // but the discovery-session device-type constants differ between macOS SDK versions,
    // whereas this call enumerates every video device on every SDK. Suppress the
    // deprecation warning rather than track SDK-specific device-type lists.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    NSArray<AVCaptureDevice*>* devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
#pragma clang diagnostic pop

    int index = 0;
    for(AVCaptureDevice* device in devices)
    {
      std::string name = "Camera " + std::to_string(index);
      NSString* localized = device.localizedName;
      if(localized)
      {
        const char* utf8 = [localized UTF8String];
        if(utf8)
          name = utf8;
      }

      auto resolutions = enumerateAVFoundationResolutions(device);

      // Current/default ("native") format = the device's activeFormat.
      std::pair<uint32_t, uint32_t> native{0, 0};
      if(AVCaptureDeviceFormat* active = device.activeFormat)
      {
        if(CMFormatDescriptionRef desc = active.formatDescription)
        {
          const CMVideoDimensions dim = CMVideoFormatDescriptionGetDimensions(desc);
          if(dim.width > 0 && dim.height > 0)
            native = {static_cast<uint32_t>(dim.width), static_cast<uint32_t>(dim.height)};
        }
      }

      result.push_back({std::to_string(index), name, std::move(resolutions), native});
      ++index;
    }
  }

  // No devices (or none enumerable): offer a few bare indices so the control is
  // never empty, matching the other platforms' fallback.
  if(result.empty())
    for(int i = 0; i < 4; i++)
      result.push_back({std::to_string(i), "Camera " + std::to_string(i)});

  return result;
}

#endif // __APPLE__
