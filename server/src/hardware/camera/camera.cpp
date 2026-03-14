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
#include "list/cameralist.hpp"
#include "list/cameralisttablemodel.hpp"
#include "capture/cameracapture.hpp"
#include "capture/localcameracapture.hpp"
#include "capture/ipcameracapture.hpp"
#include "../../core/objectproperty.tpp"
#include "../../world/world.hpp"
#include "../../core/attributes.hpp"
#include "../../utils/displayname.hpp"

// ─── Constructor ─────────────────────────────────────────────────────────────

Camera::Camera(World& world, std::string_view _id)
  : IdObject(world, _id)
  , name       {this, "name",         id.value(),           PropertyFlags::ReadWrite | PropertyFlags::Store | PropertyFlags::ScriptReadOnly}
  , type       {this, "type",         CameraType::Local,    PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const CameraType& /*newValue*/)
      {
        applySettings();
      }}
  , device     {this, "device",       std::string{"0"},     PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& /*newValue*/)
      {
        applySettings();
      }}
  , enabled    {this, "enabled",      false,                PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const bool& value)
      {
        if(value)
          startCapture();
        else
          stopCapture();
      }}
  , streamUrl  {this, "stream_url",   std::string{},        PropertyFlags::ReadOnly | PropertyFlags::NoStore}
  , frameWidth {this, "frame_width",  0u,                   PropertyFlags::ReadOnly | PropertyFlags::NoStore}
  , frameHeight{this, "frame_height", 0u,                   PropertyFlags::ReadOnly | PropertyFlags::NoStore}
  , fps        {this, "fps",          25.0,                 PropertyFlags::ReadWrite | PropertyFlags::Store}
{
  const bool editable = contains(m_world.state.value(), WorldState::Edit);

  Attributes::addDisplayName(name, DisplayName::Object::name);
  Attributes::addEnabled(name, editable);
  m_interfaceItems.add(name);

  Attributes::addValues(type, cameraTypeValues);
  Attributes::addEnabled(type, editable);
  m_interfaceItems.add(type);

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

  switch(type.value())
  {
    case CameraType::Local:
      m_capture = std::make_unique<LocalCameraCapture>(device.value(), fps.value());
      break;

    case CameraType::RTSP:
    case CameraType::MJPEG:
      m_capture = std::make_unique<IpCameraCapture>(device.value(), fps.value());
      break;
  }

  if(!m_capture->open())
  {
    // No suitable log message exists for camera open failure — reset silently.
    m_capture.reset();
    return;
  }

  frameWidth .setValueInternal(m_capture->width());
  frameHeight.setValueInternal(m_capture->height());

  streamUrl.setValueInternal("/camera/" + id.value() + "/stream");

  m_running = true;
  m_captureThread = std::thread(&Camera::captureLoop, this);
}

void Camera::stopCapture()
{
  if(!m_running)
    return;

  m_running = false;
  if(m_capture)
    m_capture->interrupt();

  if(m_captureThread.joinable())
    m_captureThread.join();

  m_capture.reset();
  streamUrl  .setValueInternal("");
  frameWidth .setValueInternal(0u);
  frameHeight.setValueInternal(0u);
}

void Camera::captureLoop()
{
  std::vector<uint8_t> jpegBuf;

  while(m_running)
  {
    if(!m_capture->readJpeg(jpegBuf))
      break;  // interrupted or unrecoverable error

    publishFrame(jpegBuf);
  }
}

void Camera::publishFrame(std::vector<uint8_t> jpegData)
{
  std::lock_guard<std::mutex> lock(m_subscriberMutex);
  for(auto& [subId, cb] : m_subscribers)
    cb(jpegData);
}
