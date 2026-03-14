/**
 * server/src/hardware/camera/camera.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
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
#include <string>
#include <string_view>
#include <functional>

class CameraList;
class CameraCapture;

class Camera : public IdObject
{
  CLASS_ID("camera")
  DEFAULT_ID("camera")
  CREATE(Camera)

  friend class CameraList;

public:
  Property<std::string>  name;
  Property<CameraType>   type;
  Property<std::string>  device;
  Property<bool>         enabled;
  Property<std::string>  streamUrl;
  Property<uint32_t>     frameWidth;
  Property<uint32_t>     frameHeight;
  Property<double>       fps;

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
  // ── Capture ────────────────────────────────────────────────────────────
  std::unique_ptr<CameraCapture>                    m_capture;
  std::atomic<bool>                                 m_running{false};
  std::thread                                       m_captureThread;

  std::mutex                                        m_subscriberMutex;
  std::vector<std::pair<uint64_t, FrameCallback>>  m_subscribers;
  uint64_t                                          m_nextSubscriberId{1};

  // ── Local-camera device attribute data ────────────────────────────────
  // Populated once at construction; stable thereafter so string_views are safe.
  std::vector<std::string>       m_deviceValues;    ///< ["0", "1", …]
  std::vector<std::string>       m_deviceNamesStr;  ///< owns display-name strings
  std::vector<std::string_view>  m_deviceNames;     ///< views into m_deviceNamesStr

  void startCapture();
  void stopCapture();
  void captureLoop();
  void publishFrame(std::vector<uint8_t> jpegData);
  void applySettings();

  /// Push or clear the Values / AliasKeys / AliasValues attributes on
  /// the `device` property based on the current camera type.
  void updateDeviceAttribute();
};

#endif
