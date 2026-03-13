/**
 * server/src/hardware/camera/capture/cameracapture.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_CAMERACAPTURE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAPTURE_CAMERACAPTURE_HPP

#include <cstdint>
#include <vector>

/**
 * @brief Abstract base for all camera capture backends.
 *
 * Subclasses handle the platform-specific details of opening a device and
 * delivering frames encoded as JPEG bytes. The capture thread in Camera calls
 * readJpeg() in a loop; the result is forwarded to all frame subscribers.
 */
class CameraCapture
{
public:
  virtual ~CameraCapture() = default;

  /** Open the device. Returns false on failure. */
  [[nodiscard]] virtual bool open() = 0;

  /** Width of captured frames in pixels (valid after open()). */
  virtual uint32_t width()  const = 0;

  /** Height of captured frames in pixels (valid after open()). */
  virtual uint32_t height() const = 0;

  /**
   * @brief Block until a new frame is available and fill @p jpegOut with
   *        JPEG-encoded bytes.
   *
   * @return true on success, false on unrecoverable error or after interrupt().
   */
  [[nodiscard]] virtual bool readJpeg(std::vector<uint8_t>& jpegOut) = 0;

  /**
   * @brief Signal the capture backend to stop blocking in readJpeg().
   *        Called from a different thread than readJpeg().
   */
  virtual void interrupt() = 0;
};

#endif
