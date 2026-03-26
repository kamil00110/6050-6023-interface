/**
 * server/src/hardware/camera/camera.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */

#include "camera.hpp"
#include "cameraenumerator.hpp"
#include "list/cameralist.hpp"
#include "list/cameralisttablemodel.hpp"
#include "capture/cameracapture.hpp"
#include "capture/localcameracapture.hpp"
#include "capture/ipcameracapture.hpp"
#include "../../core/objectproperty.tpp"
#include "../../core/eventloop.hpp"
#include "../../world/world.hpp"
#include "../../core/attributes.hpp"
#include "../../utils/displayname.hpp"
#include "../../log/log.hpp"
#include <sstream>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h>
  #include <objbase.h>
#endif

// ─── Constructor ─────────────────────────────────────────────────────────────

Camera::Camera(World& world, std::string_view _id)
  : IdObject(world, _id)
  , name       {this, "name",         id.value(),        PropertyFlags::ReadWrite | PropertyFlags::Store | PropertyFlags::ScriptReadOnly}
  , type       {this, "type",         CameraType::Local, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const CameraType& newValue)
      {
        updateDeviceAttribute();
        if(newValue == CameraType::Local)
        {
          // Reset device to first local camera index so the combo box
          // never sees a URL string as its current value.
          device.setValueInternal(
            m_deviceValues.empty() ? std::string{"0"} : m_deviceValues.front());
        }
        if(enabled)
          stopCapture();
      }}
  , device     {this, "device",       std::string{"0"},  PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& /*newValue*/)
      {
        applySettings();
      }}
  , enabled    {this, "enabled",      false,             PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const bool& value)
      {
        if(value)
          startCapture();
        else
          stopCapture();
      }}
  , streamUrl  {this, "stream_url",   std::string{},     PropertyFlags::ReadOnly | PropertyFlags::NoStore}
  , frameWidth {this, "frame_width",  0u,                PropertyFlags::ReadOnly | PropertyFlags::NoStore}
  , frameHeight{this, "frame_height", 0u,                PropertyFlags::ReadOnly | PropertyFlags::NoStore}
  , fps        {this, "fps",          25.0,              PropertyFlags::ReadWrite | PropertyFlags::Store}
{
  const bool editable = contains(m_world.state.value(), WorldState::Edit);

  for(const auto& cam : enumerateLocalCameras())
  {
    m_deviceValues.push_back(cam.device);
    m_deviceNamesStr.push_back(cam.name);
  }
  m_deviceNames.reserve(m_deviceNamesStr.size());
  for(const auto& s : m_deviceNamesStr)
    m_deviceNames.emplace_back(s);

  Attributes::addDisplayName(name, DisplayName::Object::name);
  Attributes::addEnabled(name, editable);
  m_interfaceItems.add(name);

  Attributes::addValues(type, cameraTypeValues);
  Attributes::addEnabled(type, editable);
  m_interfaceItems.add(type);

  {
    const bool isLocal = (type.value() == CameraType::Local);
    const std::vector<std::string>*      vp = isLocal ? &m_deviceValues : nullptr;
    const std::vector<std::string_view>* np = isLocal ? &m_deviceNames  : nullptr;
    Attributes::addValues (device, vp);
    Attributes::addAliases(device, vp, np);
  }
  Attributes::addEnabled(device, editable);
  m_interfaceItems.add(device);

  Attributes::addEnabled(fps, editable);
  Attributes::addMinMax(fps, 1.0, 60.0);
  m_interfaceItems.add(fps);

  m_interfaceItems.add(enabled);
  m_interfaceItems.add(streamUrl);
  m_interfaceItems.add(frameWidth);
  m_interfaceItems.add(frameHeight);
}

// ─── Lifecycle ───────────────────────────────────────────────────────────────

Camera::~Camera()
{
  stopCapture();
}

void Camera::addToWorld()
{
  IdObject::addToWorld();
  m_world.cameras->addObject(shared_ptr<Camera>());
}

void Camera::loaded()
{
  IdObject::loaded();
  if(enabled)
    startCapture();
}

void Camera::destroying()
{
  stopCapture();
  m_world.cameras->removeObject(shared_ptr<Camera>());
  IdObject::destroying();
}

void Camera::worldEvent(WorldState worldState, WorldEvent worldEvent)
{
  IdObject::worldEvent(worldState, worldEvent);
  const bool editable = contains(worldState, WorldState::Edit);
  Attributes::setEnabled(name,   editable);
  Attributes::setEnabled(type,   editable);
  Attributes::setEnabled(device, editable);
  Attributes::setEnabled(fps,    editable);
}

// ─── Frame subscriber API ────────────────────────────────────────────────────

uint64_t Camera::addFrameSubscriber(FrameCallback cb)
{
  std::lock_guard<std::mutex> lock(m_subscriberMutex);
  const uint64_t subscriberId = m_nextSubscriberId++;
  m_subscribers.emplace_back(subscriberId, std::move(cb));
  return subscriberId;
}

void Camera::removeFrameSubscriber(uint64_t subscriberId)
{
  std::lock_guard<std::mutex> lock(m_subscriberMutex);
  m_subscribers.erase(
    std::remove_if(m_subscribers.begin(), m_subscribers.end(),
      [subscriberId](const auto& p){ return p.first == subscriberId; }),
    m_subscribers.end());
}

// ─── Private helpers ─────────────────────────────────────────────────────────

void Camera::updateDeviceAttribute()
{
  const bool isLocal = (type.value() == CameraType::Local);
  const std::vector<std::string>*      vp = isLocal ? &m_deviceValues : nullptr;
  const std::vector<std::string_view>* np = isLocal ? &m_deviceNames  : nullptr;
  Attributes::setValues (device, vp);
  Attributes::setAliases(device, vp, np);
}

void Camera::applySettings()
{
  if(enabled)
  {
    stopCapture();
    startCapture();
  }
}

void Camera::startCapture()
{
  if(m_running)
    return;

  try
  {
    switch(type.value())
    {
      case CameraType::Local:
        m_capture = std::make_unique<LocalCameraCapture>(device.value(), fps.value());
        break;
      case CameraType::RTSP:
      case CameraType::MJPEG:
        m_capture = std::make_unique<IpCameraCapture>(device.value(), fps.value(), *this);
        break;
    }
  }
  catch(const std::exception& e)
  {
    Log::log(*this, LogMessage::E9999_X, std::string("camera init exception: ") + e.what());
    m_capture.reset();
    return;
  }
  catch(...)
  {
    Log::log(*this, LogMessage::E9999_X,
      std::string("camera init unknown exception for device: ") + device.value());
    m_capture.reset();
    return;
  }

  m_running = true;

#ifdef _WIN32
  struct ThreadArgs { Camera* self; };
  auto* args = new ThreadArgs{this};
  HANDLE h = CreateThread(nullptr, 4 * 1024 * 1024,
    [](LPVOID param) -> DWORD
    {
      auto* a = static_cast<ThreadArgs*>(param);
      a->self->captureLoop();
      delete a;
      return 0;
    },
    args, 0, nullptr);

  if(h)
  {
    m_captureThreadHandle = h;
  }
  else
  {
    delete args;
    m_running = false;
    m_capture.reset();
    Log::log(*this, LogMessage::E9999_X,
      std::string("CreateThread failed for camera: ") + id.value());
  }
#else
  m_captureThread = std::thread(&Camera::captureLoop, this);
#endif
}

void Camera::stopCapture()
{
  if(!m_running)
    return;

  m_running = false;
  if(m_capture)
    m_capture->interrupt();

#ifdef _WIN32
  if(m_captureThreadHandle)
  {
    WaitForSingleObject(m_captureThreadHandle, INFINITE);
    CloseHandle(m_captureThreadHandle);
    m_captureThreadHandle = nullptr;
  }
#else
  if(m_captureThread.joinable())
    m_captureThread.join();
#endif

  m_capture.reset();
  streamUrl  .setValueInternal("");
  frameWidth .setValueInternal(0u);
  frameHeight.setValueInternal(0u);
}

void Camera::captureLoop()
{
#ifdef _WIN32
  const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if(FAILED(hr) && hr != RPC_E_CHANGED_MODE)
    Log::log(*this, LogMessage::E9999_X,
      std::string("CoInitializeEx failed on capture thread, HRESULT: ") + std::to_string(hr));
#endif

  // open() on the capture thread — correct COM apartment, event loop stays unblocked
  bool openOk = false;
  try { openOk = m_capture && m_capture->open(); }
  catch(const std::exception& e)
  {
    Log::log(*this, LogMessage::E9999_X, std::string("camera open exception: ") + e.what());
  }
  catch(...) {}

  if(!openOk)
  {
    Log::log(*this, LogMessage::E9999_X,
      std::string("camera open failed for: [") + device.value()
      + "] type=" + std::to_string(static_cast<int>(type.value())));
    m_running = false;
#ifdef _WIN32
    CoUninitialize();
#endif
    return;
  }

  // Post width/height/streamUrl back to the event loop — use a typed weak_ptr
  // because weak_from_this() returns weak_ptr<Object>, not weak_ptr<Camera>.
  const uint32_t    w    = m_capture->width();
  const uint32_t    h    = m_capture->height();
  const std::string path = "/camera/" + id.value() + "/stream";

  EventLoop::call(
    [weak = std::weak_ptr<Camera>(std::static_pointer_cast<Camera>(shared_from_this())),
     w, h, path]()
    {
      if(auto self = weak.lock())
      {
        if(!self->m_running)
          return;
        self->frameWidth .setValueInternal(w);
        self->frameHeight.setValueInternal(h);
        self->streamUrl  .setValueInternal(path);
      }
    });

  captureLoopBody();

#ifdef _WIN32
  CoUninitialize();
#endif
}

void Camera::captureLoopBody()
{
#ifdef _WIN32
  MSG msg{};
  PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE);

  std::vector<uint8_t> jpegBuf;
  __try
  {
    while(m_running)
    {
      while(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
      if(!m_capture->readJpeg(jpegBuf))
      {
        Log::log(*this, LogMessage::E9999_X,
          std::string("camera readJpeg failed, stopping capture for: ") + id.value());
        break;
      }
      publishFrame(jpegBuf);
    }
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    std::ostringstream oss;
    oss << std::hex << GetExceptionCode();
    Log::log(*this, LogMessage::E9999_X,
      std::string("capture loop SEH exception 0x") + oss.str()
      + " for camera: " + id.value());
  }
#else
  try
  {
    std::vector<uint8_t> jpegBuf;
    while(m_running)
    {
      if(!m_capture->readJpeg(jpegBuf))
      {
        Log::log(*this, LogMessage::E9999_X,
          std::string("camera readJpeg failed, stopping capture for: ") + id.value());
        break;
      }
      publishFrame(jpegBuf);
    }
  }
  catch(const std::exception& e)
  {
    Log::log(*this, LogMessage::E9999_X, std::string("capture loop exception: ") + e.what());
  }
  catch(...)
  {
    Log::log(*this, LogMessage::E9999_X,
      std::string("capture loop unknown exception for camera: ") + id.value());
  }
#endif
}

void Camera::publishFrame(std::vector<uint8_t> jpegData)
{
  std::lock_guard<std::mutex> lock(m_subscriberMutex);
  for(auto& [subId, cb] : m_subscribers)
    cb(jpegData);
}
