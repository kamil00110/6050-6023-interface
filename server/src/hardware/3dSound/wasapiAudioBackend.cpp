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
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <Windows.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

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
  double delaySeconds;
  std::atomic<bool> isPlaying;
  std::atomic<bool> shouldStop;
  std::thread playbackThread;
  UINT32 bufferFrameCount;
  
  AudioStream() 
    : device(nullptr)
    , audioClient(nullptr)
    , renderClient(nullptr)
    , format(nullptr)
    , targetChannel(0)
    , volume(1.0)
    , delaySeconds(0.0)
    , isPlaying(false)
    , shouldStop(false)
    , bufferFrameCount(0)
  {}
  
  ~AudioStream()
  {
    cleanup();
  }
  
  void cleanup()
  {
    if(renderClient)
    {
      renderClient->Release();
      renderClient = nullptr;
    }
    if(audioClient)
    {
      audioClient->Stop();
      audioClient->Release();
      audioClient = nullptr;
    }
    if(format)
    {
      CoTaskMemFree(format);
      format = nullptr;
    }
    if(device)
    {
      device->Release();
      device = nullptr;
    }
  }
};

struct WASAPIAudioBackend::Impl
{
  IMMDeviceEnumerator* deviceEnumerator;
  std::map<std::string, std::vector<std::unique_ptr<AudioStream>>> activeStreams;
  std::mutex streamsMutex;
  bool initialized;
  
  Impl() : deviceEnumerator(nullptr), initialized(false) {}
  
  ~Impl()
  {
    std::lock_guard<std::mutex> lock(streamsMutex);
    activeStreams.clear();
    
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

WASAPIAudioBackend::WASAPIAudioBackend()
{
  m_impl = std::make_unique<Impl>();
}

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
    else
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Unsupported bit depth: ") + std::to_string(header.bitsPerSample));
      return false;
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

static IMMDevice* getDeviceByName(IMMDeviceEnumerator* enumerator, const std::string& deviceName)
{
  if(deviceName.empty())
  {
    // Get default device
    IMMDevice* device = nullptr;
    HRESULT hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if(FAILED(hr))
      return nullptr;
    return device;
  }
  
  // Enumerate devices to find by name
  IMMDeviceCollection* collection = nullptr;
  HRESULT hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
  if(FAILED(hr))
    return nullptr;
  
  UINT count = 0;
  collection->GetCount(&count);
  
  for(UINT i = 0; i < count; i++)
  {
    IMMDevice* device = nullptr;
    hr = collection->Item(i, &device);
    if(FAILED(hr))
      continue;
    
    IPropertyStore* props = nullptr;
    hr = device->OpenPropertyStore(STGM_READ, &props);
    if(SUCCEEDED(hr))
    {
      PROPVARIANT varName;
      PropVariantInit(&varName);
      
      hr = props->GetValue(PKEY_Device_FriendlyName, &varName);
      if(SUCCEEDED(hr))
      {
        // Convert wide string to narrow string
        int size = WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, nullptr, 0, nullptr, nullptr);
        std::string name(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, &name[0], size, nullptr, nullptr);
        
        PropVariantClear(&varName);
        props->Release();
        
        if(name.find(deviceName) != std::string::npos)
        {
          collection->Release();
          return device;
        }
      }
      props->Release();
    }
    device->Release();
  }
  
  collection->Release();
  return nullptr;
}

static void playbackThreadFunc(AudioStream* stream, const AudioFileData& audioData, 
                               bool looping, double speed)
{
  HRESULT hr;
  
  // Start the audio client
  hr = stream->audioClient->Start();
  if(FAILED(hr))
  {
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Failed to start audio client"));
    stream->isPlaying = false;
    return;
  }
  
  stream->isPlaying = true;
  
  // Apply initial delay if specified
  if(stream->delaySeconds > 0.0)
  {
    Sleep(static_cast<DWORD>(stream->delaySeconds * 1000.0));
  }
  
  // Calculate samples per frame for source audio
  const uint32_t sourceSamplesPerFrame = audioData.channels;
  const uint32_t totalSourceFrames = static_cast<uint32_t>(audioData.samples.size()) / sourceSamplesPerFrame;
  
  // Playback position
  double sourcePosition = 0.0;
  
  do
  {
    // Check if we should stop
    if(stream->shouldStop)
      break;
    
    // Get buffer padding (how much is already in the buffer)
    UINT32 numFramesPadding = 0;
    hr = stream->audioClient->GetCurrentPadding(&numFramesPadding);
    if(FAILED(hr))
      break;
    
    // Calculate available frames
    UINT32 numFramesAvailable = stream->bufferFrameCount - numFramesPadding;
    
    if(numFramesAvailable > 0)
    {
      // Get the buffer
      BYTE* pData = nullptr;
      hr = stream->renderClient->GetBuffer(numFramesAvailable, &pData);
      if(FAILED(hr))
        break;
      
      // Fill the buffer
      float* floatBuffer = reinterpret_cast<float*>(pData);
      const uint32_t outputChannels = stream->format->nChannels;
      
      for(UINT32 frame = 0; frame < numFramesAvailable; frame++)
      {
        // Get source frame index
        uint32_t sourceFrame = static_cast<uint32_t>(sourcePosition);
        
        // Check if we've reached the end
        if(sourceFrame >= totalSourceFrames)
        {
          if(looping)
          {
            sourcePosition = 0.0;
            sourceFrame = 0;
          }
          else
          {
            // Fill remaining with silence
            for(uint32_t ch = 0; ch < outputChannels; ch++)
            {
              floatBuffer[frame * outputChannels + ch] = 0.0f;
            }
            continue;
          }
        }
        
        // Linear interpolation for speed adjustment
        double frac = sourcePosition - sourceFrame;
        uint32_t nextFrame = sourceFrame + 1;
        if(nextFrame >= totalSourceFrames)
          nextFrame = looping ? 0 : sourceFrame;
        
        // Get source sample (handle mono/stereo source)
        float sourceSample = 0.0f;
        if(audioData.channels == 1)
        {
          // Mono source
          float s1 = audioData.samples[sourceFrame];
          float s2 = audioData.samples[nextFrame];
          sourceSample = s1 + (s2 - s1) * static_cast<float>(frac);
        }
        else
        {
          // Stereo or multi-channel source - take first channel
          float s1 = audioData.samples[sourceFrame * sourceSamplesPerFrame];
          float s2 = audioData.samples[nextFrame * sourceSamplesPerFrame];
          sourceSample = s1 + (s2 - s1) * static_cast<float>(frac);
        }
        
        // Apply volume
        sourceSample *= static_cast<float>(stream->volume);
        
        // Write to output channels
        for(uint32_t ch = 0; ch < outputChannels; ch++)
        {
          if(stream->targetChannel < 0 || static_cast<uint32_t>(stream->targetChannel) == ch)
          {
            // Write to this channel
            floatBuffer[frame * outputChannels + ch] = sourceSample;
          }
          else
          {
            // Silence other channels
            floatBuffer[frame * outputChannels + ch] = 0.0f;
          }
        }
        
        // Advance source position with speed adjustment
        sourcePosition += speed;
      }
      
      // Release the buffer
      hr = stream->renderClient->ReleaseBuffer(numFramesAvailable, 0);
      if(FAILED(hr))
        break;
      
      // Check if playback is complete
      if(!looping && sourcePosition >= totalSourceFrames)
      {
        break;
      }
    }
    
    // Sleep a bit to avoid busy waiting
    Sleep(1);
    
  } while(!stream->shouldStop);
  
  // Stop the audio client
  stream->audioClient->Stop();
  stream->isPlaying = false;
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
  
  const AudioFileData& audioData = audioIt->second;
  
  // Stop if already playing
  if(m_activeSounds.count(soundId))
  {
    stopSound(soundId);
  }
  
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("Starting playback: ") + soundId + 
    " on " + std::to_string(outputs.size()) + " output(s)");
  
  std::lock_guard<std::mutex> lock(m_impl->streamsMutex);
  auto& streams = m_impl->activeStreams[soundId];
  
  // Create audio stream for each output configuration
  for(const auto& config : outputs)
  {
    auto stream = std::make_unique<AudioStream>();
    
    // Get the audio device
    stream->device = getDeviceByName(m_impl->deviceEnumerator, config.deviceName);
    if(!stream->device)
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Failed to get device: ") + config.deviceName);
      continue;
    }
    
    // Activate audio client
    HRESULT hr = stream->device->Activate(
      __uuidof(IAudioClient),
      CLSCTX_ALL,
      nullptr,
      (void**)&stream->audioClient);
    
    if(FAILED(hr))
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Failed to activate audio client"));
      continue;
    }
    
    // Get the mix format
    hr = stream->audioClient->GetMixFormat(&stream->format);
    if(FAILED(hr))
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Failed to get mix format"));
      continue;
    }
    
    // Initialize audio client in shared mode
    REFERENCE_TIME requestedDuration = 10000000; // 1 second
    hr = stream->audioClient->Initialize(
      AUDCLNT_SHAREMODE_SHARED,
      0,
      requestedDuration,
      0,
      stream->format,
      nullptr);
    
    if(FAILED(hr))
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Failed to initialize audio client"));
      continue;
    }
    
    // Get buffer size
    hr = stream->audioClient->GetBufferSize(&stream->bufferFrameCount);
    if(FAILED(hr))
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Failed to get buffer size"));
      continue;
    }
    
    // Get render client
    hr = stream->audioClient->GetService(
      __uuidof(IAudioRenderClient),
      (void**)&stream->renderClient);
    
    if(FAILED(hr))
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Failed to get render client"));
      continue;
    }
    
    // Set stream configuration
    stream->targetChannel = config.channel;
    stream->volume = config.volume;
    stream->delaySeconds = config.delay;
    stream->shouldStop = false;
    
    // Start playback thread
    AudioStream* streamPtr = stream.get();
    stream->playbackThread = std::thread(playbackThreadFunc, streamPtr, 
                                         std::ref(audioData), looping, speed);
    
    streams.push_back(std::move(stream));
    
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Created stream for device: ") + config.deviceName +
      ", channel: " + std::to_string(config.channel) +
      ", volume: " + std::to_string(config.volume) +
      ", delay: " + std::to_string(config.delay));
  }
  
  if(streams.empty())
  {
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Failed to create any audio streams"));
    m_impl->activeStreams.erase(soundId);
    return false;
  }
  
  m_activeSounds[soundId] = true;
  return true;
}

bool WASAPIAudioBackend::stopSound(const std::string& soundId)
{
  auto it = m_activeSounds.find(soundId);
  if(it == m_activeSounds.end())
    return false;
  
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("Stopping sound: ") + soundId);
  
  std::lock_guard<std::mutex> lock(m_impl->streamsMutex);
  
  auto streamIt = m_impl->activeStreams.find(soundId);
  if(streamIt != m_impl->activeStreams.end())
  {
    // Signal all streams to stop
    for(auto& stream : streamIt->second)
    {
      stream->shouldStop = true;
    }
    
    // Wait for threads to finish
    for(auto& stream : streamIt->second)
    {
      if(stream->playbackThread.joinable())
      {
        stream->playbackThread.join();
      }
    }
    
    m_impl->activeStreams.erase(streamIt);
  }
  
  m_activeSounds.erase(it);
  return true;
}

void WASAPIAudioBackend::stopAllSounds()
{
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("Stopping all sounds"));
  
  std::lock_guard<std::mutex> lock(m_impl->streamsMutex);
  
  // Signal all streams to stop
  for(auto& [soundId, streams] : m_impl->activeStreams)
  {
    for(auto& stream : streams)
    {
      stream->shouldStop = true;
    }
  }
  
  // Wait for all threads to finish
  for(auto& [soundId, streams] : m_impl->activeStreams)
  {
    for(auto& stream : streams)
    {
      if(stream->playbackThread.joinable())
      {
        stream->playbackThread.join();
      }
    }
  }
  
  m_impl->activeStreams.clear();
  m_activeSounds.clear();
}

bool WASAPIAudioBackend::isSoundPlaying(const std::string& soundId) const
{
  return m_activeSounds.count(soundId) > 0;
}

#else // Not Windows - Stub implementation

WASAPIAudioBackend::WASAPIAudioBackend() = default;
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
