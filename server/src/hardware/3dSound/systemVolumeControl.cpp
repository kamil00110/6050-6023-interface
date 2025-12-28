/**
 * server/src/hardware/3dSound/systemVolumeControl.cpp
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

#include "systemVolumeControl.hpp"
#include "../../log/log.hpp"
#include <algorithm>

#ifdef _WIN32
#include <Windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audioclient.h>
#include <comdef.h>
#include <sstream>

struct SystemVolumeControl::Impl
{
  IMMDeviceEnumerator* deviceEnumerator;
  IAudioEndpointVolume* endpointVolume;
  bool initialized;
  
  Impl() 
    : deviceEnumerator(nullptr)
    , endpointVolume(nullptr)
    , initialized(false)
  {}
  
  ~Impl()
  {
    if(endpointVolume)
    {
      endpointVolume->Release();
      endpointVolume = nullptr;
    }
    if(deviceEnumerator)
    {
      deviceEnumerator->Release();
      deviceEnumerator = nullptr;
    }
  }
};

#endif // _WIN32

SystemVolumeControl& SystemVolumeControl::instance()
{
  static SystemVolumeControl instance;
  return instance;
}

#ifdef _WIN32

SystemVolumeControl::~SystemVolumeControl()
{
  cleanup();
}

bool SystemVolumeControl::ensureInitialized()
{
  if(!m_impl)
  {
    m_impl = std::make_unique<Impl>();
  }
  
  if(m_impl->initialized)
    return true;
  
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if(FAILED(hr) && hr != RPC_E_CHANGED_MODE)
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to initialize COM"));
    return false;
  }
  
  // Create device enumerator
  hr = CoCreateInstance(
    __uuidof(MMDeviceEnumerator),
    nullptr,
    CLSCTX_ALL,
    __uuidof(IMMDeviceEnumerator),
    (void**)&m_impl->deviceEnumerator);
  
  if(FAILED(hr))
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to create device enumerator"));
    return false;
  }
  
  // Get default audio endpoint
  IMMDevice* device = nullptr;
  hr = m_impl->deviceEnumerator->GetDefaultAudioEndpoint(
    eRender, eConsole, &device);
  
  if(FAILED(hr))
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to get default audio endpoint"));
    return false;
  }
  
  // Activate endpoint volume interface
  hr = device->Activate(
    __uuidof(IAudioEndpointVolume),
    CLSCTX_ALL,
    nullptr,
    (void**)&m_impl->endpointVolume);
  
  device->Release();
  
  if(FAILED(hr))
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to activate endpoint volume"));
    return false;
  }
  
  m_impl->initialized = true;
  Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
    std::string("System volume control initialized"));
  
  return true;
}

void SystemVolumeControl::cleanup()
{
  if(m_impl)
  {
    m_impl.reset();
  }
}

double SystemVolumeControl::getSystemVolume()
{
  if(!ensureInitialized())
    return -1.0;
  
  float volume = 0.0f;
  HRESULT hr = m_impl->endpointVolume->GetMasterVolumeLevelScalar(&volume);
  
  if(FAILED(hr))
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to get system volume"));
    return -1.0;
  }
  
  return static_cast<double>(volume);
}

bool SystemVolumeControl::setSystemVolume(double volume)
{
  if(!ensureInitialized())
    return false;
  
  // Clamp volume to valid range
  volume = std::clamp(volume, 0.0, 1.0);
  
  HRESULT hr = m_impl->endpointVolume->SetMasterVolumeLevelScalar(
    static_cast<float>(volume), nullptr);
  
  if(FAILED(hr))
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to set system volume"));
    return false;
  }
  
  Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
    std::string("System volume set to ") + std::to_string(volume * 100.0) + "%");
  
  return true;
}

bool SystemVolumeControl::getSystemMute()
{
  if(!ensureInitialized())
    return false;
  
  BOOL muted = FALSE;
  HRESULT hr = m_impl->endpointVolume->GetMute(&muted);
  
  if(FAILED(hr))
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to get mute state"));
    return false;
  }
  
  return muted != FALSE;
}

bool SystemVolumeControl::setSystemMute(bool muted)
{
  if(!ensureInitialized())
    return false;
  
  HRESULT hr = m_impl->endpointVolume->SetMute(muted ? TRUE : FALSE, nullptr);
  
  if(FAILED(hr))
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to set mute state"));
    return false;
  }
  
  Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
    std::string("System ") + (muted ? "muted" : "unmuted"));
  
  return true;
}

double SystemVolumeControl::getDeviceVolume(const std::string& deviceId)
{
  if(!ensureInitialized())
    return -1.0;
  
  IMMDevice* device = nullptr;
  HRESULT hr;
  
  if(deviceId.empty())
  {
    // Get default device
    hr = m_impl->deviceEnumerator->GetDefaultAudioEndpoint(
      eRender, eConsole, &device);
  }
  else
  {
    // Get specific device
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, deviceId.c_str(), -1, nullptr, 0);
    if(wideSize <= 0)
      return -1.0;
    
    std::vector<wchar_t> wideDeviceId(wideSize);
    MultiByteToWideChar(CP_UTF8, 0, deviceId.c_str(), -1, wideDeviceId.data(), wideSize);
    
    hr = m_impl->deviceEnumerator->GetDevice(wideDeviceId.data(), &device);
  }
  
  if(FAILED(hr))
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to get device"));
    return -1.0;
  }
  
  IAudioEndpointVolume* volume = nullptr;
  hr = device->Activate(
    __uuidof(IAudioEndpointVolume),
    CLSCTX_ALL,
    nullptr,
    (void**)&volume);
  
  device->Release();
  
  if(FAILED(hr))
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to activate device volume"));
    return -1.0;
  }
  
  float vol = 0.0f;
  hr = volume->GetMasterVolumeLevelScalar(&vol);
  volume->Release();
  
  if(FAILED(hr))
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to get device volume"));
    return -1.0;
  }
  
  return static_cast<double>(vol);
}

bool SystemVolumeControl::setDeviceVolume(const std::string& deviceId, double volume)
{
  if(!ensureInitialized())
    return false;
  
  volume = std::clamp(volume, 0.0, 1.0);
  
  IMMDevice* device = nullptr;
  HRESULT hr;
  
  if(deviceId.empty())
  {
    hr = m_impl->deviceEnumerator->GetDefaultAudioEndpoint(
      eRender, eConsole, &device);
  }
  else
  {
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, deviceId.c_str(), -1, nullptr, 0);
    if(wideSize <= 0)
      return false;
    
    std::vector<wchar_t> wideDeviceId(wideSize);
    MultiByteToWideChar(CP_UTF8, 0, deviceId.c_str(), -1, wideDeviceId.data(), wideSize);
    
    hr = m_impl->deviceEnumerator->GetDevice(wideDeviceId.data(), &device);
  }
  
  if(FAILED(hr))
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to get device"));
    return false;
  }
  
  IAudioEndpointVolume* vol = nullptr;
  hr = device->Activate(
    __uuidof(IAudioEndpointVolume),
    CLSCTX_ALL,
    nullptr,
    (void**)&vol);
  
  device->Release();
  
  if(FAILED(hr))
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to activate device volume"));
    return false;
  }
  
  hr = vol->SetMasterVolumeLevelScalar(static_cast<float>(volume), nullptr);
  vol->Release();
  
  if(FAILED(hr))
  {
    Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
      std::string("Failed to set device volume"));
    return false;
  }
  
  Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
    std::string("Device volume set to ") + std::to_string(volume * 100.0) + "%");
  
  return true;
}

#else // Not Windows - Stub implementation

SystemVolumeControl::~SystemVolumeControl()
{
  // Stub implementation
}

double SystemVolumeControl::getSystemVolume()
{
  Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
    std::string("System volume control not available on this platform"));
  return -1.0;
}

bool SystemVolumeControl::setSystemVolume(double /*volume*/)
{
  Log::log(std::string("SystemVolume"), LogMessage::I1006_X,
    std::string("System volume control not available on this platform"));
  return false;
}

bool SystemVolumeControl::getSystemMute()
{
  return false;
}

bool SystemVolumeControl::setSystemMute(bool /*muted*/)
{
  return false;
}

double SystemVolumeControl::getDeviceVolume(const std::string& /*deviceId*/)
{
  return -1.0;
}

bool SystemVolumeControl::setDeviceVolume(const std::string& /*deviceId*/, double /*volume*/)
{
  return false;
}

#endif // _WIN32
