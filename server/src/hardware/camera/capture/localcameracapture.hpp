/**
 * server/src/hardware/camera/capture/localcameracapture.hpp
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
#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_LOCALCAMERACAPTURE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_LOCALCAMERACAPTURE_HPP

#include "cameracapture.hpp"
#include <cstdint>
#include <string>
#include <memory>
#include <atomic>

class Object;
namespace cv { class VideoCapture; }

class LocalCameraCapture final : public CameraCapture
{
public:
  LocalCameraCapture(const std::string& device, double fps,
                     uint32_t reqWidth, uint32_t reqHeight,
                     int jpegQuality, bool flipVertical, bool flipHorizontal,
                     int brightness, bool applyBrightness,
                     Object& logObject);
  ~LocalCameraCapture() override;

  bool     open()    override;
  uint32_t width()   const override { return m_width;  }
  uint32_t height()  const override { return m_height; }
  bool     readJpeg(std::vector<uint8_t>& jpegOut) override;
  void     interrupt() override { m_interrupted = true; }

private:
  std::string                       m_device;
  uint32_t                          m_reqWidth{0};   ///< 0 = keep camera default
  uint32_t                          m_reqHeight{0};
  int                               m_initBrightness{0};
  bool                              m_applyBrightness{false}; ///< false = auto (force neutral)
  Object&                           m_logObject;
  std::unique_ptr<cv::VideoCapture> m_cap;
  uint32_t                          m_width{0};
  uint32_t                          m_height{0};
  std::atomic<bool>                 m_interrupted{false};

  static constexpr int k_reconnectWaitMs = 500;
};
#endif
