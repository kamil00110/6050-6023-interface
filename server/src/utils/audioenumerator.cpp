/**
 * server/src/utils/audioenumerator.cpp
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

#include "audioenumerator.hpp"
#include "../log/log.hpp"
#include "../log/logmessageexception.hpp"

#ifdef _WIN32

#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <comdef.h>
#include <sstream>

// Link required libraries
#pragma comment(lib, "ole32.lib")

// Helper to convert wide string to UTF-8
static std::string wideToUtf8(const WCHAR* wstr)
{
  if(!wstr)
    return "";
  
  int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
  if(size <= 0)
    return "";
  
  std::string result(size - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], size, nullptr, nullptr);
  return result;
}

// Helper to get channel name from channel mask
static std::string getChannelName(DWORD channelMask, int channelIndex)
{
  // Standard speaker positions
  static const struct {
    DWORD mask;
    const char* name;
  } channelNames[] = {
    { SPEAKER_FRONT_LEFT, "Front Left" },
    { SPEAKER_FRONT_RIGHT, "Front Right" },
    { SPEAKER_FRONT_CENTER, "Front Center" },
    { SPEAKER_LOW_FREQUENCY, "LFE/Subwoofer" },
    { SPEAKER_BACK_LEFT, "Back Left" },
    { SPEAKER_BACK_RIGHT, "Back Right" },
    { SPEAKER_FRONT_LEFT_OF_CENTER, "Front Left of Center" },
    { SPEAKER_FRONT_RIGHT_OF_CENTER, "Front Right of Center" },
    { SPEAKER_BACK_CENTER, "Back Center" },
    { SPEAKER_SIDE_LEFT, "Side Left" },
    { SPEAKER_SIDE_RIGHT, "Side Right" },
    { SPEAKER_TOP_CENTER, "Top Center" },
    { SPEAKER_TOP_FRONT_LEFT, "Top Front Left" },
    { SPEAKER_TOP_FRONT_CENTER, "Top Front Center" },
    { SPEAKER_TOP_FRONT_RIGHT, "Top Front Right" },
    { SPEAKER_TOP_BACK_LEFT, "Top Back Left" },
    { SPEAKER_TOP_BACK_CENTER, "Top Back Center" },
    { SPEAKER_TOP_BACK_RIGHT, "Top Back Right" }
  };
  
  // Find which channel this is based on the mask
  int currentChannel = 0;
  for(const auto& ch : channelNames)
  {
    if(channelMask & ch.mask)
    {
      if(currentChannel == channelIndex)
        return ch.name;
      currentChannel++;
    }
  }
  
  // Fallback if not found
  return "Channel " + std::to_string(channelIndex + 1);
}

// RAII wrapper for COM objects
template<typename T>
class ComPtr
{
public:
  ComPtr() : ptr(nullptr) {}
  ~ComPtr() { if(ptr) ptr->Release(); }
  
  T** operator&() { return &ptr; }
  T* operator->() { return ptr; }
  T* get() { return ptr; }
  
  ComPtr(const ComPtr&) = delete;
  ComPtr& operator=(const ComPtr&) = delete;
  
private:
  T* ptr;
};

struct AudioEnumerator::Impl
{
  bool comInitialized = false;
  
  Impl()
  {
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if(SUCCEEDED(hr))
    {
      comInitialized = true;
    }
    else if(hr == RPC_E_CHANGED_MODE)
    {
      // COM already initialized in a different mode, that's ok
      comInitialized = false;
    }
  }
  
  ~Impl()
  {
    if(comInitialized)
      CoUninitialize();
  }
};

AudioEnumerator::AudioEnumerator()
  : m_impl(std::make_unique<Impl>())
{
}

AudioEnumerator::~AudioEnumerator() = default;

std::unique_ptr<AudioEnumerator> AudioEnumerator::create()
{
  return std::unique_ptr<AudioEnumerator>(new AudioEnumerator());
}

std::vector<AudioDeviceInfo> AudioEnumerator::enumerateDevices()
{
  std::vector<AudioDeviceInfo> devices;
  
  try
  {
    // Create device enumerator
    ComPtr<IMMDeviceEnumerator> deviceEnumerator;
    HRESULT hr = CoCreateInstance(
      __uuidof(MMDeviceEnumerator),
      nullptr,
      CLSCTX_ALL,
      __uuidof(IMMDeviceEnumerator),
      (void**)&deviceEnumerator);
    
    if(FAILED(hr))
      throw std::runtime_error("Failed to create device enumerator");
    
    // Get default device
    ComPtr<IMMDevice> defaultDevice;
    std::string defaultDeviceId;
    hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice);
    if(SUCCEEDED(hr))
    {
      LPWSTR pwszID = nullptr;
      defaultDevice->GetId(&pwszID);
      if(pwszID)
      {
        defaultDeviceId = wideToUtf8(pwszID);
        CoTaskMemFree(pwszID);
      }
    }
    
    // Enumerate all output devices
    ComPtr<IMMDeviceCollection> deviceCollection;
    hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollection);
    if(FAILED(hr))
      throw std::runtime_error("Failed to enumerate audio endpoints");
    
    UINT deviceCount = 0;
    hr = deviceCollection->GetCount(&deviceCount);
    if(FAILED(hr))
      throw std::runtime_error("Failed to get device count");
    
    // Iterate through each device
    for(UINT i = 0; i < deviceCount; i++)
    {
      ComPtr<IMMDevice> device;
      hr = deviceCollection->Item(i, &device);
      if(FAILED(hr))
        continue;
      
      AudioDeviceInfo info;
      
      // Get device ID
      LPWSTR pwszID = nullptr;
      device->GetId(&pwszID);
      if(pwszID)
      {
        info.deviceId = wideToUtf8(pwszID);
        info.isDefault = (info.deviceId == defaultDeviceId);
        CoTaskMemFree(pwszID);
      }
      
      // Get device friendly name
      ComPtr<IPropertyStore> propertyStore;
      hr = device->OpenPropertyStore(STGM_READ, &propertyStore);
      if(SUCCEEDED(hr))
      {
        PROPVARIANT varName;
        PropVariantInit(&varName);
        
        hr = propertyStore->GetValue(PKEY_Device_FriendlyName, &varName);
        if(SUCCEEDED(hr) && varName.vt == VT_LPWSTR)
        {
          info.deviceName = wideToUtf8(varName.pwszVal);
        }
        PropVariantClear(&varName);
      }
      
      // Get audio client to query format
      ComPtr<IAudioClient> audioClient;
      hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient);
      if(SUCCEEDED(hr))
      {
        // Get mix format (this is what the device is currently configured for)
        WAVEFORMATEX* mixFormat = nullptr;
        hr = audioClient->GetMixFormat(&mixFormat);
        if(SUCCEEDED(hr) && mixFormat)
        {
          info.channelCount = mixFormat->nChannels;
          
          // Check if it's WAVEFORMATEXTENSIBLE (has channel mask)
          DWORD channelMask = 0;
          if(mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
          {
            WAVEFORMATEXTENSIBLE* extFormat = (WAVEFORMATEXTENSIBLE*)mixFormat;
            channelMask = extFormat->dwChannelMask;
          }
          else
          {
            // Default channel masks for common configurations
            if(mixFormat->nChannels == 1)
              channelMask = SPEAKER_FRONT_CENTER;
            else if(mixFormat->nChannels == 2)
              channelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
            else if(mixFormat->nChannels == 6)
              channelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | 
                           SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                           SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
            else if(mixFormat->nChannels == 8)
              channelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT |
                           SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
                           SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
                           SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
          }
          
          // Generate channel info
          for(WORD ch = 0; ch < mixFormat->nChannels; ch++)
          {
            AudioChannelInfo channelInfo;
            channelInfo.channelIndex = ch;
            channelInfo.channelName = getChannelName(channelMask, ch);
            info.channels.push_back(channelInfo);
          }
          
          CoTaskMemFree(mixFormat);
        }
      }
      
      devices.push_back(info);
    }
  }
  catch(const std::exception& e)
  {
    // Log error but return whatever we collected
    Log::log(LogMessage::E1001_X, std::string("Audio enumeration error: ") + e.what());
  }
  
  return devices;
}

void AudioEnumerator::logDeviceInfo()
{
  auto devices = enumerateDevices();
  
  Log::log(LogMessage::I1006_X, std::string("=== Windows Audio Devices (WASAPI) ==="));
  Log::log(LogMessage::I1006_X, std::string("Found ") + std::to_string(devices.size()) + " audio output device(s)");
  
  for(size_t i = 0; i < devices.size(); i++)
  {
    const auto& device = devices[i];
    
    std::string deviceHeader = "\n--- Device " + std::to_string(i + 1) + " ---";
    if(device.isDefault)
      deviceHeader += " [DEFAULT]";
    deviceHeader += "\nName: " + device.deviceName;
    deviceHeader += "\nID: " + device.deviceId;
    deviceHeader += "\nChannels: " + std::to_string(device.channelCount);
    
    Log::log(LogMessage::I1006_X, deviceHeader);
    
    // Log each channel
    for(const auto& channel : device.channels)
    {
      Log::log(LogMessage::I1006_X, 
        std::string("  Channel ") + std::to_string(channel.channelIndex) + ": " + channel.channelName);
    }
  }
  
  Log::log(LogMessage::I1006_X, std::string("=== End Audio Device List ==="));
}

#else // Not Windows

// Stub implementation for non-Windows platforms
AudioEnumerator::AudioEnumerator() = default;
AudioEnumerator::~AudioEnumerator() = default;

std::unique_ptr<AudioEnumerator> AudioEnumerator::create()
{
  return std::unique_ptr<AudioEnumerator>(new AudioEnumerator());
}

std::vector<AudioDeviceInfo> AudioEnumerator::enumerateDevices()
{
  return {}; // Empty list on non-Windows platforms
}

void AudioEnumerator::logDeviceInfo()
{
  Log::log(LogMessage::I1006_X, std::string("Audio enumeration not implemented for this platform"));
}

#endif // _WIN32
