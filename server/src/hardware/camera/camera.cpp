/**
 * server/src/hardware/camera/camera.cpp
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
#include <algorithm>

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

Camera::Camera(World& world, std::string_view _id)
  : IdObject(world, _id)
  , name       {this, "name",         id.value(),        PropertyFlags::ReadWrite | PropertyFlags::Store | PropertyFlags::ScriptReadOnly}
  , type       {this, "type",         CameraType::Local, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const CameraType& newValue)
      {
        updateDeviceAttribute();
        updateSpecVisibility();
        if(newValue == CameraType::Local)
        {
          device.setValueInternal(
            m_deviceValues.empty() ? std::string{"0"} : m_deviceValues.front());
        }
        updateResolutionValues();
        if(enabled)
          stopCapture();
      }}
  , device     {this, "device",       std::string{"0"},  PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& /*newValue*/)
      {
        updateResolutionValues(); // the selected camera's resolutions differ
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
  , fps        {this, "fps",          25.0,              PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const double& value)
      {
        // For an MJPEG source we request from, reconnect so the new fps is sent in
        // the request URL; otherwise fps changes take effect live (server-side rate
        // limiting) on all protocols without reconnecting.
        if(type.value() == CameraType::MJPEG && requestFromSource.value())
          applySettings();
        else if(m_capture)
          m_capture->setFps(value);
      }}
  , jpegQuality{this, "jpeg_quality", 75,                PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const int& value)
      {
        // Compression takes effect live (server-side JPEG encode); for a requested
        // MJPEG source reconnect so the new quality is sent in the request URL.
        if(type.value() == CameraType::MJPEG && requestFromSource.value())
          applySettings();
        else if(m_capture)
          m_capture->setJpegQuality(value);
      }}
  , flipVertical{this, "flip_vertical", false,           PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const bool&) { applySettings(); }}
  , flipHorizontal{this, "flip_horizontal", false,       PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const bool&) { applySettings(); }}
  , resolution {this, "resolution",   CameraResolution::Auto, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const CameraResolution&)
      {
        // Resolution needs the device re-opened to take effect.
        applySettings();
      }}
  , brightness {this, "brightness",   0,                 PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const int& value)
      {
        if(!autoBrightness.value() && m_capture)
          m_capture->setBrightness(value);
      }}
  , autoBrightness{this, "auto_brightness", true,        PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const bool& value)
      {
        // Switching back to auto resets the manual offset to neutral (0) -- the
        // camera is re-opened with brightness 0, so the previously set value is
        // not silently kept. Then enable/disable the slider accordingly.
        if(value)
          brightness.setValueInternal(0);
        updateBrightnessEnabled();
        applySettings();
      }}
  , requestFromSource{this, "request_from_source", false, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const bool&)
      {
        // Toggles whether fps/quality/resolution are appended to the MJPEG URL;
        // reconnect so it takes effect and show/hide the resolution control.
        updateSpecVisibility();
        applySettings();
      }}
{
  const bool editable = contains(m_world.state.value(), WorldState::Edit);

  for(const auto& cam : enumerateLocalCameras())
  {
    m_deviceValues.push_back(cam.device);
    m_deviceNamesStr.push_back(cam.name);
    m_deviceResolutions.push_back(cam.resolutions);
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
  // The device selector is a pick-from-list for local cameras; never let it hold
  // a free-typed value (e.g. a leftover IP URL) -- the URL has its own field.
  Attributes::addCustom(device, false);
  m_interfaceItems.add(device);

  Attributes::addEnabled(fps, editable);
  Attributes::addMinMax(fps, 1.0, 60.0);
  m_interfaceItems.add(fps);

  // jpeg_quality: 1-100
  Attributes::addEnabled(jpegQuality, editable);
  Attributes::addMinMax(jpegQuality, 1, 100);
  m_interfaceItems.add(jpegQuality);

  Attributes::addEnabled(flipVertical, editable);
  m_interfaceItems.add(flipVertical);

  Attributes::addEnabled(flipHorizontal, editable);
  m_interfaceItems.add(flipHorizontal);

  // resolution / brightness only apply to local (OpenCV) cameras; they are
  // hidden for the network types.
  const bool localType = (type.value() == CameraType::Local);

  // Add the Values as a vector (not the std::array) so the stored attribute is a
  // VectorAttribute -- updateResolutionValues() narrows it per camera via
  // setValues(std::vector), which requires a VectorAttribute.
  Attributes::addValues(resolution,
    std::vector<CameraResolution>(cameraResolutionValues.begin(), cameraResolutionValues.end()));
  Attributes::addEnabled(resolution, editable);
  Attributes::addVisible(resolution, localType);
  m_interfaceItems.add(resolution);

  Attributes::addEnabled(autoBrightness, editable);
  Attributes::addVisible(autoBrightness, localType);
  m_interfaceItems.add(autoBrightness);

  Attributes::addEnabled(brightness, editable && !autoBrightness.value());
  Attributes::addMinMax(brightness, -100, 100); // signed: -100 darkest .. 0 neutral .. 100 brightest
  Attributes::addVisible(brightness, localType);
  m_interfaceItems.add(brightness);

  // MJPEG only: append fps/quality/resolution to the stream URL as query params.
  Attributes::addEnabled(requestFromSource, editable);
  Attributes::addVisible(requestFromSource, type.value() == CameraType::MJPEG);
  m_interfaceItems.add(requestFromSource);

  m_interfaceItems.add(enabled);
  m_interfaceItems.add(streamUrl);
  m_interfaceItems.add(frameWidth);
  m_interfaceItems.add(frameHeight);

  updateResolutionValues(); // narrow the resolution list to the default camera
}

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
  updateResolutionValues(); // reflect the loaded device's resolution list
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
  Attributes::setEnabled(name,        editable);
  Attributes::setEnabled(type,        editable);
  Attributes::setEnabled(device,      editable);
  Attributes::setEnabled(fps,         editable);
  Attributes::setEnabled(jpegQuality,    editable);
  Attributes::setEnabled(flipVertical,   editable);
  Attributes::setEnabled(flipHorizontal, editable);
  Attributes::setEnabled(resolution,     editable);
  Attributes::setEnabled(autoBrightness, editable);
  Attributes::setEnabled(requestFromSource, editable);
  updateBrightnessEnabled(); // brightness = editable && manual mode
}

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

std::string Camera::deviceDisplayName() const
{
  if(type.value() == CameraType::Local)
  {
    for(size_t i = 0; i < m_deviceValues.size() && i < m_deviceNamesStr.size(); ++i)
      if(m_deviceValues[i] == device.value())
        return m_deviceNamesStr[i];
  }
  return device.value();
}

void Camera::updateDeviceAttribute()
{
  const bool isLocal = (type.value() == CameraType::Local);
  const std::vector<std::string>*      vp = isLocal ? &m_deviceValues : nullptr;
  const std::vector<std::string_view>* np = isLocal ? &m_deviceNames  : nullptr;
  Attributes::setValues (device, vp);
  Attributes::setAliases(device, vp, np);
}

void Camera::updateResolutionValues()
{
  // Offer only the resolutions the selected local camera declares (mapped onto
  // the fixed CameraResolution set), always keeping Auto. For non-local cameras,
  // or when nothing maps, keep the full list so the control is never empty and
  // IP cameras can still request any size.
  std::vector<std::pair<uint32_t, uint32_t>> declared;
  if(type.value() == CameraType::Local)
  {
    for(size_t i = 0; i < m_deviceValues.size() && i < m_deviceResolutions.size(); ++i)
      if(m_deviceValues[i] == device.value())
      {
        declared = m_deviceResolutions[i];
        break;
      }
  }

  std::vector<CameraResolution> values;
  values.push_back(CameraResolution::Auto);
  for(const CameraResolution r : cameraResolutionValues)
  {
    if(r == CameraResolution::Auto)
      continue;
    const auto size = toResolutionSize(r);
    for(const auto& d : declared)
      if(d.first == size.first && d.second == size.second)
      {
        values.push_back(r);
        break;
      }
  }

  if(values.size() == 1) // nothing matched / not local -> keep the full list
    values.assign(cameraResolutionValues.begin(), cameraResolutionValues.end());

  // If the current selection is no longer on offer, fall back to Auto.
  if(std::find(values.begin(), values.end(), resolution.value()) == values.end())
    resolution.setValueInternal(CameraResolution::Auto);

  Attributes::setValues(resolution, std::move(values));
}

std::vector<std::pair<uint32_t, uint32_t>> Camera::deviceResolutions(const std::string& dev) const
{
  if(type.value() == CameraType::Local)
    for(size_t i = 0; i < m_deviceValues.size() && i < m_deviceResolutions.size(); ++i)
      if(m_deviceValues[i] == dev)
        return m_deviceResolutions[i];
  return {};
}

void Camera::updateSpecVisibility()
{
  const bool isLocal = (type.value() == CameraType::Local);
  const bool isMjpeg = (type.value() == CameraType::MJPEG);
  // Resolution applies to local cameras, and to an MJPEG source we request from.
  Attributes::setVisible(resolution, isLocal || (isMjpeg && requestFromSource.value()));
  Attributes::setVisible(brightness, isLocal);
  Attributes::setVisible(autoBrightness, isLocal);
  Attributes::setVisible(requestFromSource, isMjpeg);
}

void Camera::updateBrightnessEnabled()
{
  const bool editable = contains(m_world.state.value(), WorldState::Edit);
  Attributes::setEnabled(brightness, editable && !autoBrightness.value());
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
      {
        // Auto: hand the capture the device's declared sizes (largest-first) so
        // it probes for the largest that actually delivers video; if the device
        // declared nothing, fall back to a standard descending set. An explicit
        // choice is passed as the single size.
        std::vector<std::pair<uint32_t, uint32_t>> candidates;
        if(resolution.value() == CameraResolution::Auto)
        {
          candidates = deviceResolutions(device.value());
          if(candidates.empty())
            candidates = {{1920, 1080}, {1280, 720}, {1024, 768}, {800, 600}, {640, 480}, {640, 360}};
        }
        else
        {
          candidates.push_back(toResolutionSize(resolution.value()));
        }
        m_capture = std::make_unique<LocalCameraCapture>(
          device.value(), fps.value(),
          std::move(candidates),
          jpegQuality.value(), flipVertical.value(), flipHorizontal.value(),
          brightness.value(), !autoBrightness.value(), *this);
        break;
      }

      case CameraType::RTSP:
      case CameraType::MJPEG:
      case CameraType::RTMP:
      case CameraType::HLS:
      {
        const auto [reqW, reqH] = toResolutionSize(resolution.value());
        const bool appendSpecs = (type.value() == CameraType::MJPEG) && requestFromSource.value();
        m_capture = std::make_unique<IpCameraCapture>(
          device.value(), fps.value(), reqW, reqH,
          jpegQuality.value(), flipVertical.value(), flipHorizontal.value(),
          appendSpecs, *this);
        break;
      }
    }
  }
  catch(const std::exception& e)
  {
    LOG_DEBUG("camera init exception:", e.what());
    m_capture.reset();
    return;
  }
  catch(...)
  {
    LOG_DEBUG("camera init unknown exception for device:", device.value());
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
    m_captureThreadHandle = h;
  else
  {
    delete args;
    m_running = false;
    m_capture.reset();
    LOG_DEBUG("CreateThread failed for camera:", id.value());
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

  Log::log(*this, LogMessage::N2010_CAMERA_CAPTURE_STOPPED);
}

void Camera::captureLoop()
{
#ifdef _WIN32
  const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if(FAILED(hr) && hr != RPC_E_CHANGED_MODE)
    LOG_DEBUG("CoInitializeEx failed on capture thread, HRESULT:", std::to_string(hr));
#endif

  bool openOk = false;
  try { openOk = m_capture && m_capture->open(); }
  catch(const std::exception& e)
  {
    LOG_DEBUG("camera open exception:", e.what());
  }
  catch(...) {}

  if(!openOk)
  {
    Log::log(*this, LogMessage::E2035_OPENING_CAMERA_X_FAILED, deviceDisplayName());
    m_running = false;
#ifdef _WIN32
    CoUninitialize();
#endif
    return;
  }

  Log::log(*this, LogMessage::N2009_CAMERA_CAPTURE_STARTED);

  const uint32_t    w    = m_capture->width();
  const uint32_t    h    = m_capture->height();
  const std::string path = "/camera/" + id.value() + "/stream";

  EventLoop::call(
    [weak = std::weak_ptr<Camera>(
        std::static_pointer_cast<Camera>(shared_from_this())),
     w, h, path]()
    {
      if(auto self = weak.lock())
      {
        if(!self->m_running) return;
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
        Log::log(*this, LogMessage::E2036_CAMERA_STREAM_LOST);
        break;
      }
      publishFrame(jpegBuf);
    }
  }
  __except(EXCEPTION_EXECUTE_HANDLER)
  {
    LOG_DEBUG("capture loop SEH exception for camera:", id.value());
  }
#else
  try
  {
    std::vector<uint8_t> jpegBuf;
    while(m_running)
    {
      if(!m_capture->readJpeg(jpegBuf))
      {
        Log::log(*this, LogMessage::E2036_CAMERA_STREAM_LOST);
        break;
      }
      publishFrame(jpegBuf);
    }
  }
  catch(const std::exception& e)
  {
    LOG_DEBUG("capture loop exception:", e.what());
  }
  catch(...)
  {
    LOG_DEBUG("capture loop unknown exception for camera:", id.value());
  }
#endif
}

void Camera::publishFrame(std::vector<uint8_t> jpegData)
{
  // Copy the subscriber list under the lock, then invoke the callbacks without
  // it held: a subscriber must be free to unsubscribe (which locks the same
  // mutex) from within its callback without deadlocking.
  std::vector<std::pair<uint64_t, FrameCallback>> subscribers;
  {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    subscribers = m_subscribers;
  }
  for(auto& [subId, cb] : subscribers)
    cb(jpegData);
}
