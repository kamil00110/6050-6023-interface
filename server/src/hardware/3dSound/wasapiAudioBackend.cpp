/**
 * server/src/hardware/3dSound/wasapiAudioBackend.cpp
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

#include "wasapiAudioBackend.hpp"
#include "../../log/log.hpp"
#include <fstream>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <thread>
#include <atomic>

// Minimal WAV header structure
struct WAVHeader
{
  char riff[4];           // "RIFF"
  uint32_t fileSize;
  char wave[4];           // "WAVE"
  char fmt[4];            // "fmt "
  uint32_t fmtSize;
  uint16_t audioFormat;   // 1 = PCM
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  char data[4];           // "data"
  uint32_t dataSize;
};

struct AudioStream
{
  IMMDevice* device;
  IAudioClient* audioClient;
  IAudioRenderClient* renderClient;
  WAVEFORMATEX* format;
  int targetChannel;
  double volume;
  double delay;
  std::atomic<bool> isPlaying;
  std::thread playbackThread;
};

struct WASAPIAudioBackend::Impl
{
  IMMDeviceEnumerator* deviceEnumerator;
  std::map<std::string, std::vector<std::unique_ptr<AudioStream>>> activeStreams;
  bool initialized;
  
  Impl() : deviceEnumerator(nullptr), initialized(false) {}
  
  ~Impl()
  {
    if(deviceEnumerator)
    {
      deviceEnumerator->Release();
      deviceEnumerator = nullptr;
    }
  }
};

#endif // _WIN32

WASAPIAudioBackend& WASAPIAudioBackend::instance()
{
  static WASAPIAudioBackend instance;
  return instance;
}

#ifdef _WIN32

WASAPIAudioBackend::~WASAPIAudioBackend()
{
  shutdown();
}

bool WASAPIAudioBackend::initialize()
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
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Failed to initialize COM"));
    return false;
  }
  
  hr = CoCreateInstance(
    __uuidof(MMDeviceEnumerator),
    nullptr,
    CLSCTX_ALL,
    __uuidof(IMMDeviceEnumerator),
    (void**)&m_impl->deviceEnumerator);
  
  if(FAILED(hr))
  {
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Failed to create device enumerator"));
    return false;
  }
  
  m_impl->initialized = true;
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("WASAPI backend initialized"));
  
  return true;
}

void WASAPIAudioBackend::shutdown()
{
  if(!m_impl || !m_impl->initialized)
    return;
  
  stopAllSounds();
  m_audioFiles.clear();
  
  if(m_impl->deviceEnumerator)
  {
    m_impl->deviceEnumerator->Release();
    m_impl->deviceEnumerator = nullptr;
  }
  
  m_impl->initialized = false;
  
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("WASAPI backend shut down"));
}

bool WASAPIAudioBackend::loadAudioFile(const std::string& filePath, const std::string& soundId)
{
  try
  {
    std::ifstream file(filePath, std::ios::binary);
    if(!file.is_open())
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Failed to open audio file: ") + filePath);
      return false;
    }
    
    // Read WAV header
    WAVHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));
    
    // Validate WAV file
    if(std::memcmp(header.riff, "RIFF", 4) != 0 || 
       std::memcmp(header.wave, "WAVE", 4) != 0)
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Invalid WAV file: ") + filePath);
      return false;
    }
    
    // Read audio data
    AudioFileData audioData;
    audioData.sampleRate = header.sampleRate;
    audioData.channels = header.numChannels;
    audioData.bitsPerSample = header.bitsPerSample;
    
    // Calculate number of samples
    uint32_t numSamples = header.dataSize / (header.bitsPerSample / 8);
    audioData.samples.resize(numSamples);
    
    // Read and convert to float
    if(header.bitsPerSample == 16)
    {
      std::vector<int16_t> int16Data(numSamples);
      file.read(reinterpret_cast<char*>(int16Data.data()), header.dataSize);
      
      for(size_t i = 0; i < numSamples; i++)
      {
        audioData.samples[i] = int16Data[i] / 32768.0f;
      }
    }
    else if(header.bitsPerSample == 32)
    {
      file.read(reinterpret_cast<char*>(audioData.samples.data()), header.dataSize);
    }
    
    m_audioFiles[soundId] = audioData;
    
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Loaded audio file: ") + soundId + 
      " (" + std::to_string(numSamples) + " samples, " +
      std::to_string(header.sampleRate) + " Hz, " +
      std::to_string(header.numChannels) + " channels)");
    
    return true;
  }
  catch(const std::exception& e)
  {
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Error loading audio file: ") + e.what());
    return false;
  }
}

void WASAPIAudioBackend::unloadAudioFile(const std::string& soundId)
{
  auto it = m_audioFiles.find(soundId);
  if(it != m_audioFiles.end())
  {
    m_audioFiles.erase(it);
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Unloaded audio file: ") + soundId);
  }
}

bool WASAPIAudioBackend::playSound(const std::string& soundId, 
                                     const std::vector<AudioStreamConfig>& outputs,
                                     bool looping,
                                     double speed)
{
  if(!m_impl || !m_impl->initialized)
  {
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Backend not initialized"));
    return false;
  }
  
  // Check if audio file is loaded
  auto audioIt = m_audioFiles.find(soundId);
  if(audioIt == m_audioFiles.end())
  {
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Audio file not loaded: ") + soundId);
    return false;
  }
  
  // Stop if already playing
  if(m_activeSounds.count(soundId))
  {
    stopSound(soundId);
  }
  
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("Starting playback: ") + soundId + 
    " on " + std::to_string(outputs.size()) + " output(s)");
  
  // TODO: Create audio streams for each output
  // For now, just mark as playing
  m_activeSounds[soundId] = true;
  
  // TODO: Implement actual WASAPI playback
  // This would involve:
  // 1. Opening audio client for each device/channel
  // 2. Starting playback threads
  // 3. Writing audio data with proper volume and delay
  // 4. Handling looping and speed adjustment
  
  return true;
}

bool WASAPIAudioBackend::stopSound(const std::string& soundId)
{
  auto it = m_activeSounds.find(soundId);
  if(it == m_activeSounds.end())
    return false;
  
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("Stopping sound: ") + soundId);
  
  // TODO: Stop actual playback streams
  
  m_activeSounds.erase(it);
  return true;
}

void WASAPIAudioBackend::stopAllSounds()
{
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("Stopping all sounds"));
  
  // TODO: Stop all playback streams
  
  m_activeSounds.clear();
}

bool WASAPIAudioBackend::isSoundPlaying(const std::string& soundId) const
{
  return m_activeSounds.count(soundId) > 0;
}

#else // Not Windows - Stub implementation

WASAPIAudioBackend::~WASAPIAudioBackend() = default;

bool WASAPIAudioBackend::initialize()
{
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("WASAPI not available on this platform"));
  return false;
}

void WASAPIAudioBackend::shutdown()
{
  // Nothing to do
}

bool WASAPIAudioBackend::loadAudioFile(const std::string& /*filePath*/, const std::string& /*soundId*/)
{
  return false;
}

void WASAPIAudioBackend::unloadAudioFile(const std::string& /*soundId*/)
{
  // Nothing to do
}

bool WASAPIAudioBackend::playSound(const std::string& /*soundId*/, 
                                     const std::vector<AudioStreamConfig>& /*outputs*/,
                                     bool /*looping*/,
                                     double /*speed*/)
{
  return false;
}

bool WASAPIAudioBackend::stopSound(const std::string& /*soundId*/)
{
  return false;
}

void WASAPIAudioBackend::stopAllSounds()
{
  // Nothing to do
}

bool WASAPIAudioBackend::isSoundPlaying(const std::string& /*soundId*/) const
{
  return false;
}

#endif // _WIN32
