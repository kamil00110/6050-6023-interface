/**
 * server/src/hardware/camera/camera.hpp
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

#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAMERA_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_CAMERA_HPP

#include "../../core/idobject.hpp"
#include "../../core/property.hpp"
#include "cameratype.hpp"
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <functional>

class CameraList;
class CameraCapture;

/**
 * @brief Represents a single camera source.
 *
 * A Camera object encapsulates either a local USB/V4L2 camera (CameraType::Local)
 * or an IP camera accessed via RTSP or plain MJPEG-over-HTTP.
 *
 * The server captures frames in a background thread and exposes them as a
 * MJPEG stream at  /camera/<id>/stream  on the embedded HTTP server so that
 * any number of Qt clients (or browsers) can connect without opening the
 * underlying capture device more than once.
 *
 * The read-only `streamUrl` property is populated once the stream is ready
 * and is sent to connected clients via the normal binary property-sync
 * mechanism, so clients do not need to construct the URL themselves.
 */
class Camera : public IdObject
{
  CLASS_ID("camera")
  DEFAULT_ID("camera")
  CREATE(Camera)

  friend class CameraList;

public:
  // ── Properties exposed to clients ────────────────────────────────────────
  Property<std::string>  name;        ///< User-visible label
  Property<CameraType>   type;        ///< Local / RTSP / MJPEG
  Property<std::string>  device;      ///< Device index ("0") or URL
  Property<bool>         enabled;     ///< Activate/deactivate capture
  Property<std::string>  streamUrl;   ///< Read-only: MJPEG HTTP URL for clients
  Property<uint32_t>     frameWidth;  ///< Captured frame width  (informational)
  Property<uint32_t>     frameHeight; ///< Captured frame height (informational)
  Property<double>       fps;         ///< Requested capture frame-rate

  // ── Frame subscriber API (used by MJPEGStreamer) ──────────────────────────
  using FrameCallback = std::function<void(std::vector<uint8_t> jpegData)>;
  uint64_t addFrameSubscriber(FrameCallback cb);
  void     removeFrameSubscriber(uint64_t id);

  Camera(World& world, std::string_view _id);
  ~Camera() override;

protected:
  void addToWorld()  override;
  void loaded()      override;
  void destroying()  override;
  void worldEvent(WorldState state, WorldEvent event) override;

private:
  std::unique_ptr<CameraCapture> m_capture;
  std::atomic<bool>              m_running{false};
  std::thread                    m_captureThread;

  std::mutex                                          m_subscriberMutex;
  std::vector<std::pair<uint64_t, FrameCallback>>    m_subscribers;
  uint64_t                                            m_nextSubscriberId{1};

  void startCapture();
  void stopCapture();
  void captureLoop();
  void publishFrame(std::vector<uint8_t> jpegData);

  void applySettings();   ///< (Re-)create capture object from current properties
};

#endif
