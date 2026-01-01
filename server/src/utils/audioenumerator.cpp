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

// ============================================================================
// WINDOWS IMPLEMENTATION (WASAPI)
// ============================================================================
#ifdef _WIN32

#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <comdef.h>

#pragma comment(lib, "ole32.lib")

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

static std::string getChannelName(DWORD channelMask, int channelIndex)
{
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
  
  return "Channel " + std::to_string(channelIndex + 1);
}

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

static bool initializeCOM()
{
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if(SUCCEEDED(hr))
    return true;
  if(hr == RPC_E_CHANGED_MODE)
    return false;
  return false;
}

std::vector<AudioDeviceInfo> AudioEnumerator::enumerateDevices()
{
  std::vector<AudioDeviceInfo> devices;
  bool comInitialized = initializeCOM();
  
  try
  {
    ComPtr<IMMDeviceEnumerator> deviceEnumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
      __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator);
    
    if(FAILED(hr))
      throw std::runtime_error("Failed to create device enumerator");
    
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
    
    ComPtr<IMMDeviceCollection> deviceCollection;
    hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollection);
    if(FAILED(hr))
      throw std::runtime_error("Failed to enumerate audio endpoints");
    
    UINT deviceCount = 0;
    hr = deviceCollection->GetCount(&deviceCount);
    if(FAILED(hr))
      throw std::runtime_error("Failed to get device count");
    
    for(UINT i = 0; i < deviceCount; i++)
    {
      ComPtr<IMMDevice> device;
      hr = deviceCollection->Item(i, &device);
      if(FAILED(hr))
        continue;
      
      AudioDeviceInfo info;
      
      LPWSTR pwszID = nullptr;
      device->GetId(&pwszID);
      if(pwszID)
      {
        info.deviceId = wideToUtf8(pwszID);
        info.isDefault = (info.deviceId == defaultDeviceId);
        CoTaskMemFree(pwszID);
      }
      
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
      
      ComPtr<IAudioClient> audioClient;
      hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient);
      if(SUCCEEDED(hr))
      {
        WAVEFORMATEX* mixFormat = nullptr;
        hr = audioClient->GetMixFormat(&mixFormat);
        if(SUCCEEDED(hr) && mixFormat)
        {
          info.channelCount = mixFormat->nChannels;
          
          DWORD channelMask = 0;
          if(mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
          {
            WAVEFORMATEXTENSIBLE* extFormat = (WAVEFORMATEXTENSIBLE*)mixFormat;
            channelMask = extFormat->dwChannelMask;
          }
          else
          {
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
    Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X, 
      std::string("Audio enumeration error: ") + e.what());
  }
  
  if(comInitialized)
    CoUninitialize();
  
  return devices;
}

void AudioEnumerator::logDevices()
{
  auto devices = enumerateDevices();
  
  Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X, 
    std::string("=== Windows Audio Devices (WASAPI) ==="));
  Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X, 
    std::string("Found ") + std::to_string(devices.size()) + " audio output device(s)");
  
  for(size_t i = 0; i < devices.size(); i++)
  {
    const auto& device = devices[i];
    
    std::string deviceHeader = "\n--- Device " + std::to_string(i + 1) + " ---";
    if(device.isDefault)
      deviceHeader += " [DEFAULT]";
    deviceHeader += "\nName: " + device.deviceName;
    deviceHeader += "\nID: " + device.deviceId;
    deviceHeader += "\nChannels: " + std::to_string(device.channelCount);
    
    Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X, deviceHeader);
    
    for(const auto& channel : device.channels)
    {
      Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X, 
        std::string("  Channel ") + std::to_string(channel.channelIndex) + ": " + channel.channelName);
    }
  }
  
  Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X, 
    std::string("=== End Audio Device List ==="));
}

std::string AudioEnumerator::getSpeakerName(const std::string& deviceId)
{
  auto devices = enumerateDevices();
  for(const auto& device : devices)
  {
    if(device.deviceId == deviceId)
      return device.deviceName;
  }
  return "Sound controller missing";
}

std::vector<std::string> AudioEnumerator::listSpeakerIds()
{
  std::vector<std::string> ids;
  auto devices = enumerateDevices();
  for(const auto& device : devices)
  {
    ids.push_back(device.deviceId);
  }
  return ids;
}

uint32_t AudioEnumerator::getSpeakerChannels(const std::string& deviceId)
{
  auto devices = enumerateDevices();
  for(const auto& device : devices)
  {
    if(device.deviceId == deviceId)
      return device.channelCount;
  }
  return 0;
}

std::vector<AudioChannelInfo> AudioEnumerator::getSpeakerChannelInfo(const std::string& deviceId)
{
  auto devices = enumerateDevices();
  for(const auto& device : devices)
  {
    if(device.deviceId == deviceId)
      return device.channels;
  }
  return {};
}

// ============================================================================
// MACOS IMPLEMENTATION (CoreAudio)
// ============================================================================
#elif defined(__APPLE__)

#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>

static std::string getChannelName(int channelIndex, int totalChannels)
{
  if(totalChannels == 1) return "Mono";
  if(totalChannels == 2)
    return (channelIndex == 0) ? "Left" : "Right";
  
  const char* names[] = {
    "Front Left", "Front Right", "Front Center", 
    "LFE/Subwoofer", "Rear Left", "Rear Right",
    "Side Left", "Side Right"
  };
  if(channelIndex < 8) return names[channelIndex];
  return "Channel " + std::to_string(channelIndex + 1);
}

std::vector<AudioDeviceInfo> AudioEnumerator::enumerateDevices()
{
  std::vector<AudioDeviceInfo> devices;
  
  try
  {
    AudioDeviceID defaultDeviceId = kAudioDeviceUnknown;
    AudioObjectPropertyAddress propertyAddress = {
      kAudioHardwarePropertyDefaultOutputDevice,
      kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMain
    };
    
    UInt32 size = sizeof(AudioDeviceID);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress,
                              0, NULL, &size, &defaultDeviceId);
    
    propertyAddress.mSelector = kAudioHardwarePropertyDevices;
    AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress,
                                  0, NULL, &size);
    
    int deviceCount = size / sizeof(AudioDeviceID);
    std::vector<AudioDeviceID> deviceIds(deviceCount);
    
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress,
                              0, NULL, &size, deviceIds.data());
    
    for(AudioDeviceID deviceId : deviceIds)
    {
      propertyAddress.mSelector = kAudioDevicePropertyStreamConfiguration;
      propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
      
      AudioObjectGetPropertyDataSize(deviceId, &propertyAddress, 0, NULL, &size);
      AudioBufferList* bufferList = (AudioBufferList*)malloc(size);
      AudioObjectGetPropertyData(deviceId, &propertyAddress, 0, NULL, &size, bufferList);
      
      UInt32 outputChannels = 0;
      for(UInt32 i = 0; i < bufferList->mNumberBuffers; i++)
        outputChannels += bufferList->mBuffers[i].mNumberChannels;
      
      free(bufferList);
      
      if(outputChannels == 0)
        continue;
      
      AudioDeviceInfo info;
      
      CFStringRef uidRef = NULL;
      propertyAddress.mSelector = kAudioDevicePropertyDeviceUID;
      propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
      size = sizeof(CFStringRef);
      
      AudioObjectGetPropertyData(deviceId, &propertyAddress, 0, NULL, &size, &uidRef);
      
      if(uidRef)
      {
        char uid[256];
        CFStringGetCString(uidRef, uid, sizeof(uid), kCFStringEncodingUTF8);
        info.deviceId = std::string("coreaudio:") + uid;
        CFRelease(uidRef);
      }
      else
      {
        info.deviceId = "coreaudio:" + std::to_string(deviceId);
      }
      
      CFStringRef nameRef = NULL;
      propertyAddress.mSelector = kAudioDevicePropertyDeviceNameCFString;
      size = sizeof(CFStringRef);
      
      AudioObjectGetPropertyData(deviceId, &propertyAddress, 0, NULL, &size, &nameRef);
      
      if(nameRef)
      {
        char name[256];
        CFStringGetCString(nameRef, name, sizeof(name), kCFStringEncodingUTF8);
        info.deviceName = name;
        CFRelease(nameRef);
      }
      
      info.channelCount = outputChannels;
      info.isDefault = (deviceId == defaultDeviceId);
      
      for(UInt32 ch = 0; ch < outputChannels; ch++)
      {
        AudioChannelInfo channelInfo;
        channelInfo.channelIndex = ch;
        channelInfo.channelName = getChannelName(ch, outputChannels);
        info.channels.push_back(channelInfo);
      }
      
      devices.push_back(info);
    }
  }
  catch(const std::exception& e)
  {
    Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X,
      std::string("CoreAudio enumeration error: ") + e.what());
  }
  
  return devices;
}

void AudioEnumerator::logDevices()
{
  auto devices = enumerateDevices();
  Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X,
    std::string("=== macOS Audio Devices (CoreAudio) ==="));
  Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X,
    std::string("Found ") + std::to_string(devices.size()) + " audio output device(s)");
  
  for(size_t i = 0; i < devices.size(); i++)
  {
    const auto& device = devices[i];
    std::string deviceHeader = "\n--- Device " + std::to_string(i + 1) + " ---";
    if(device.isDefault)
      deviceHeader += " [DEFAULT]";
    deviceHeader += "\nName: " + device.deviceName;
    deviceHeader += "\nID: " + device.deviceId;
    deviceHeader += "\nChannels: " + std::to_string(device.channelCount);
    Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X, deviceHeader);
    
    for(const auto& channel : device.channels)
    {
      Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X,
        std::string("  Channel ") + std::to_string(channel.channelIndex) + 
        ": " + channel.channelName);
    }
  }
  Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X,
    std::string("=== End Audio Device List ==="));
}

std::string AudioEnumerator::getSpeakerName(const std::string& deviceId)
{
  auto devices = enumerateDevices();
  for(const auto& device : devices)
  {
    if(device.deviceId == deviceId)
      return device.deviceName;
  }
  return "Sound controller missing";
}

std::vector<std::string> AudioEnumerator::listSpeakerIds()
{
  std::vector<std::string> ids;
  auto devices = enumerateDevices();
  for(const auto& device : devices)
  {
    ids.push_back(device.deviceId);
  }
  return ids;
}

uint32_t AudioEnumerator::getSpeakerChannels(const std::string& deviceId)
{
  auto devices = enumerateDevices();
  for(const auto& device : devices)
  {
    if(device.deviceId == deviceId)
      return device.channelCount;
  }
  return 0;
}

std::vector<AudioChannelInfo> AudioEnumerator::getSpeakerChannelInfo(const std::string& deviceId)
{
  auto devices = enumerateDevices();
  for(const auto& device : devices)
  {
    if(device.deviceId == deviceId)
      return device.channels;
  }
  return {};
}

// ============================================================================
// LINUX IMPLEMENTATION (ALSA)
// ============================================================================
#elif defined(__linux__)

#include <alsa/asoundlib.h>
#include <cstring>

static std::string getChannelName(int channelIndex, int totalChannels)
{
  if(totalChannels == 1) return "Mono";
  if(totalChannels == 2)
    return (channelIndex == 0) ? "Left" : "Right";
  
  const char* names[] = {
    "Front Left", "Front Right", "Front Center", 
    "LFE/Subwoofer", "Rear Left", "Rear Right",
    "Side Left", "Side Right"
  };
  if(channelIndex < 8) return names[channelIndex];
  return "Channel " + std::to_string(channelIndex + 1);
}

std::vector<AudioDeviceInfo> AudioEnumerator::enumerateDevices()
{
  std::vector<AudioDeviceInfo> devices;
  
  try
  {
    // First, try to enumerate hardware cards directly
    int card = -1;
    while(snd_card_next(&card) >= 0 && card >= 0)
    {
      char hwDevice[32];
      snprintf(hwDevice, sizeof(hwDevice), "hw:%d", card);
      
      snd_ctl_t* ctl = nullptr;
      if(snd_ctl_open(&ctl, hwDevice, 0) < 0)
        continue;
      
      snd_ctl_card_info_t* cardInfo;
      snd_ctl_card_info_alloca(&cardInfo);
      
      if(snd_ctl_card_info(ctl, cardInfo) >= 0)
      {
        int dev = -1;
        while(snd_ctl_pcm_next_device(ctl, &dev) >= 0 && dev >= 0)
        {
          snd_pcm_info_t* pcmInfo;
          snd_pcm_info_alloca(&pcmInfo);
          snd_pcm_info_set_device(pcmInfo, dev);
          snd_pcm_info_set_subdevice(pcmInfo, 0);
          snd_pcm_info_set_stream(pcmInfo, SND_PCM_STREAM_PLAYBACK);
          
          if(snd_ctl_pcm_info(ctl, pcmInfo) >= 0)
          {
            char deviceName[64];
            snprintf(deviceName, sizeof(deviceName), "hw:%d,%d", card, dev);
            
            snd_pcm_t* pcm = nullptr;
            if(snd_pcm_open(&pcm, deviceName, SND_PCM_STREAM_PLAYBACK, 0) >= 0)
            {
              snd_pcm_hw_params_t* params;
              snd_pcm_hw_params_alloca(&params);
              
              if(snd_pcm_hw_params_any(pcm, params) >= 0)
              {
                unsigned int maxChannels = 0;
                if(snd_pcm_hw_params_get_channels_max(params, &maxChannels) >= 0 &&
                   maxChannels > 0 && maxChannels <= 32)
                {
                  AudioDeviceInfo info;
                  info.deviceId = std::string("alsa:") + deviceName;
                  
                  const char* cardName = snd_ctl_card_info_get_name(cardInfo);
                  const char* pcmName = snd_pcm_info_get_name(pcmInfo);
                  
                  if(cardName && pcmName)
                    info.deviceName = std::string(cardName) + " - " + pcmName;
                  else if(cardName)
                    info.deviceName = cardName;
                  else
                    info.deviceName = deviceName;
                  
                  info.channelCount = maxChannels;
                  info.isDefault = (card == 0 && dev == 0);
                  
                  for(unsigned int ch = 0; ch < maxChannels; ch++)
                  {
                    AudioChannelInfo channelInfo;
                    channelInfo.channelIndex = ch;
                    channelInfo.channelName = getChannelName(ch, maxChannels);
                    info.channels.push_back(channelInfo);
                  }
                  
                  devices.push_back(info);
                }
              }
              
              snd_pcm_close(pcm);
            }
          }
        }
      }
      
      snd_ctl_close(ctl);
    }
    
    // Add PulseAudio device if available and no hardware devices found
    if(devices.empty())
    {
      snd_pcm_t* pcm = nullptr;
      const char* pulseDevices[] = { "pulse", "default" };
      
      for(const char* devName : pulseDevices)
      {
        if(snd_pcm_open(&pcm, devName, SND_PCM_STREAM_PLAYBACK, 0) >= 0)
        {
          snd_pcm_hw_params_t* params;
          snd_pcm_hw_params_alloca(&params);
          
          if(snd_pcm_hw_params_any(pcm, params) >= 0)
          {
            unsigned int maxChannels = 0;
            if(snd_pcm_hw_params_get_channels_max(params, &maxChannels) >= 0 &&
               maxChannels > 0 && maxChannels <= 32)
            {
              AudioDeviceInfo info;
              info.deviceId = std::string("alsa:") + devName;
              info.deviceName = (strcmp(devName, "pulse") == 0) ? 
                                "PulseAudio" : "Default Audio Device";
              info.channelCount = (maxChannels > 8) ? 2 : maxChannels; // Cap pulse at stereo typically
              info.isDefault = true;
              
              for(unsigned int ch = 0; ch < info.channelCount; ch++)
              {
                AudioChannelInfo channelInfo;
                channelInfo.channelIndex = ch;
                channelInfo.channelName = getChannelName(ch, info.channelCount);
                info.channels.push_back(channelInfo);
              }
              
              devices.push_back(info);
              snd_pcm_close(pcm);
              break; // Only add one of pulse/default
            }
          }
          
          snd_pcm_close(pcm);
        }
      }
    }
    
    // Last resort: use device name hints
    if(devices.empty())
    {
      Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X,
        std::string("No hardware devices found, trying hints..."));
      
      void** hints = nullptr;
      if(snd_device_name_hint(-1, "pcm", &hints) >= 0 && hints)
      {
        for(void** hint = hints; *hint != nullptr; hint++)
        {
          char* name = snd_device_name_get_hint(*hint, "NAME");
          char* desc = snd_device_name_get_hint(*hint, "DESC");
          char* ioid = snd_device_name_get_hint(*hint, "IOID");
          
          if(!name || (ioid && strcmp(ioid, "Input") == 0))
          {
            if(name) free(name);
            if(desc) free(desc);
            if(ioid) free(ioid);
            continue;
          }
          
          std::string nameStr(name);
          // Only try plughw, hw, or default/pulse devices
          if(nameStr.find("plughw:") == 0 || nameStr.find("hw:") == 0 || 
             nameStr == "default" || nameStr == "pulse")
          {
            snd_pcm_t* pcm = nullptr;
            if(snd_pcm_open(&pcm, name, SND_PCM_STREAM_PLAYBACK, 0) >= 0)
            {
              snd_pcm_hw_params_t* params;
              snd_pcm_hw_params_alloca(&params);
              
              if(snd_pcm_hw_params_any(pcm, params) >= 0)
              {
                unsigned int maxChannels = 0;
                if(snd_pcm_hw_params_get_channels_max(params, &maxChannels) >= 0 &&
                   maxChannels > 0 && maxChannels <= 32)
                {
                  AudioDeviceInfo info;
                  info.deviceId = std::string("alsa:") + name;
                  
                  if(desc)
                  {
                    std::string descStr(desc);
                    size_t newlinePos = descStr.find('\n');
                    if(newlinePos != std::string::npos)
                      descStr = descStr.substr(0, newlinePos);
                    info.deviceName = descStr;
                  }
                  else
                  {
                    info.deviceName = name;
                  }
                  
                  info.channelCount = maxChannels;
                  info.isDefault = (nameStr == "default" || nameStr == "pulse");
                  
                  for(unsigned int ch = 0; ch < maxChannels; ch++)
                  {
                    AudioChannelInfo channelInfo;
                    channelInfo.channelIndex = ch;
                    channelInfo.channelName = getChannelName(ch, maxChannels);
                    info.channels.push_back(channelInfo);
                  }
                  
                  devices.push_back(info);
                }
              }
              
              snd_pcm_close(pcm);
            }
          }
          
          free(name);
          if(desc) free(desc);
          if(ioid) free(ioid);
        }
        
        snd_device_name_free_hint(hints);
      }
    }
  }
  catch(const std::exception& e)
  {
    Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X,
      std::string("ALSA enumeration error: ") + e.what());
  }
  
  return devices;
}

void AudioEnumerator::logDevices()
{
  auto devices = enumerateDevices();
  Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X,
    std::string("=== Linux Audio Devices (ALSA) ==="));
  Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X,
    std::string("Found ") + std::to_string(devices.size()) + " audio output device(s)");
  
  for(size_t i = 0; i < devices.size(); i++)
  {
    const auto& device = devices[i];
    std::string deviceHeader = "\n--- Device " + std::to_string(i + 1) + " ---";
    if(device.isDefault)
      deviceHeader += " [DEFAULT]";
    deviceHeader += "\nName: " + device.deviceName;
    deviceHeader += "\nID: " + device.deviceId;
    deviceHeader += "\nChannels: " + std::to_string(device.channelCount);
    Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X, deviceHeader);
    
    for(const auto& channel : device.channels)
    {
      Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X,
        std::string("  Channel ") + std::to_string(channel.channelIndex) + 
        ": " + channel.channelName);
    }
  }
  Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X,
    std::string("=== End Audio Device List ==="));
}

std::string AudioEnumerator::getSpeakerName(const std::string& deviceId)
{
  auto devices = enumerateDevices();
  for(const auto& device : devices)
  {
    if(device.deviceId == deviceId)
      return device.deviceName;
  }
  return "Sound controller missing";
}

std::vector<std::string> AudioEnumerator::listSpeakerIds()
{
  std::vector<std::string> ids;
  auto devices = enumerateDevices();
  for(const auto& device : devices)
  {
    ids.push_back(device.deviceId);
  }
  return ids;
}

uint32_t AudioEnumerator::getSpeakerChannels(const std::string& deviceId)
{
  auto devices = enumerateDevices();
  for(const auto& device : devices)
  {
    if(device.deviceId == deviceId)
      return device.channelCount;
  }
  return 0;
}

std::vector<AudioChannelInfo> AudioEnumerator::getSpeakerChannelInfo(const std::string& deviceId)
{
  auto devices = enumerateDevices();
  for(const auto& device : devices)
  {
    if(device.deviceId == deviceId)
      return device.channels;
  }
  return {};
}

// ============================================================================
// FALLBACK IMPLEMENTATION (Other platforms)
// ============================================================================
#else

std::vector<AudioDeviceInfo> AudioEnumerator::enumerateDevices()
{
  return {};
}

void AudioEnumerator::logDevices()
{
  Log::log(std::string("AudioEnumerator"), LogMessage::I1006_X, 
    std::string("Audio enumeration not implemented for this platform"));
}

std::string AudioEnumerator::getSpeakerName(const std::string& /*deviceId*/)
{
  return "Sound controller missing";
}

std::vector<std::string> AudioEnumerator::listSpeakerIds()
{
  return {};
}

uint32_t AudioEnumerator::getSpeakerChannels(const std::string& /*deviceId*/)
{
  return 0;
}

std::vector<AudioChannelInfo> AudioEnumerator::getSpeakerChannelInfo(const std::string& /*deviceId*/)
{
  return {};
}

#endif
