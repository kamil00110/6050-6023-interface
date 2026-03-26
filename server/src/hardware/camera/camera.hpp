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
#include <future>
#include <vector>
#include <string>
#include <string_view>
#include <functional>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h>
#endif

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
  Property<uint32_t>     maxWidth;   ///< 0 = no limit, scale down if source is larger
  Property<uint32_t>     maxHeight;  ///< 0 = no limit
  Property<int>          jpegQuality; ///< 1-100, default 75

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
  std::unique_ptr<CameraCapture>                    m_capture;
  std::atomic<bool>                                 m_running{false};
#ifdef _WIN32
  HANDLE                                            m_captureThreadHandle{nullptr};
#else
  std::thread                                       m_captureThread;
#endif
  std::mutex                                        m_subscriberMutex;
  std::vector<std::pair<uint64_t, FrameCallback>>  m_subscribers;
  uint64_t                                          m_nextSubscriberId{1};

  std::vector<std::string>       m_deviceValues;
  std::vector<std::string>       m_deviceNamesStr;
  std::vector<std::string_view>  m_deviceNames;

  void startCapture();
  void stopCapture();
  void captureLoop();
  void captureLoopBody();
  void publishFrame(std::vector<uint8_t> jpegData);
  void applySettings();
  void updateDeviceAttribute();
};
#endif
