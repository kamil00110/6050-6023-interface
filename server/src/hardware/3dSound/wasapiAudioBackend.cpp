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
#include "formats/audioFormat.hpp"
#include "formats/wavFormat.hpp"
#include "formats/mp3Format.hpp"
#include "formats/oggFormat.hpp"
#include "formats/flacFormat.hpp"
#include "formats/w8vFormat.hpp"
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
#include <sstream>

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
  UINT64 startSample;
  std::shared_ptr<std::condition_variable> startSignal;
  std::shared_ptr<std::mutex> startMutex;
  std::shared_ptr<std::atomic<int>> readyCount;
  std::shared_ptr<std::atomic<int>> totalStreams;
  
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
    , startSample(0)
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
  
static bool formatsRegistered = false;
  if(!formatsRegistered)
  {
    AudioFormatFactory::instance().registerLoader(
      std::make_unique<WAVFormatLoader>());
    
    AudioFormatFactory::instance().registerLoader(
      std::make_unique<MP3FormatLoader>());
    
    AudioFormatFactory::instance().registerLoader(
      std::make_unique<OGGFormatLoader>());

    AudioFormatFactory::instance().registerLoader(
      std::make_unique<FLACFormatLoader>());

    AudioFormatFactory::instance().registerLoader(
      std::make_unique<W8VFormatLoader>());
    
    formatsRegistered = true;
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
bool WASAPIAudioBackend::loadAudioFile(const std::string& filePath, 
                                       const std::string& soundId)
{
  AudioFileData audioData;
  std::string error;
  
  // Use format factory to load the file
  if(!AudioFormatFactory::instance().loadAudioFile(filePath, audioData, error))
  {
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Failed to load audio file: ") + error);
    return false;
  }
  
  // Store the loaded audio data
  m_audioFiles[soundId] = audioData;
  
  uint32_t totalFrames = static_cast<uint32_t>(audioData.samples.size()) / audioData.channels;
  double durationSeconds = static_cast<double>(totalFrames) / static_cast<double>(audioData.sampleRate);
  
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("Loaded audio file: ") + soundId + 
    " (" + std::to_string(audioData.samples.size()) + " samples, " +
    std::to_string(totalFrames) + " frames, " +
    std::to_string(audioData.sampleRate) + " Hz, " +
    std::to_string(audioData.channels) + " channels, " +
    std::to_string(durationSeconds) + " seconds)");
  
  return true;
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

static IMMDevice* getDeviceById(IMMDeviceEnumerator* enumerator, const std::string& deviceId)
{
  if(deviceId.empty())
  {
    // Get default device
    IMMDevice* device = nullptr;
    HRESULT hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if(FAILED(hr))
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Failed to get default device"));
      return nullptr;
    }
    return device;
  }
  
  // Convert string to wide string for device ID
  int wideSize = MultiByteToWideChar(CP_UTF8, 0, deviceId.c_str(), -1, nullptr, 0);
  if(wideSize <= 0)
  {
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Failed to convert device ID to wide string"));
    return nullptr;
  }
  
  std::vector<wchar_t> wideDeviceId(wideSize);
  MultiByteToWideChar(CP_UTF8, 0, deviceId.c_str(), -1, wideDeviceId.data(), wideSize);
  
  // Get device by ID
  IMMDevice* device = nullptr;
  HRESULT hr = enumerator->GetDevice(wideDeviceId.data(), &device);
  
  if(FAILED(hr))
  {
    // Convert HRESULT to hex string properly
    std::ostringstream oss;
    oss << "Failed to get device by ID: " << deviceId 
        << " (HRESULT: 0x" << std::hex << static_cast<unsigned long>(hr) << ")";
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X, oss.str());
    return nullptr;
  }
  
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("Successfully got device: ") + deviceId);
  
  return device;
}

// Replace the playbackThreadFunc function with this debug version:

static void playbackThreadFunc(AudioStream* stream, const AudioFileData& audioData, 
                               bool looping, double speed)
{
  HRESULT hr;
  
  // Pre-fill delay buffer
  UINT32 delayFrames = static_cast<UINT32>(
    stream->delaySeconds * stream->format->nSamplesPerSec
  );

  while(delayFrames > 0)
  {
    UINT32 framesToWrite = (delayFrames < stream->bufferFrameCount ? delayFrames : stream->bufferFrameCount);

    BYTE* pData = nullptr;
    hr = stream->renderClient->GetBuffer(framesToWrite, &pData);
    if(FAILED(hr))
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
           std::string("GetBuffer failed during delay fill"));
      return;
    }

    memset(pData, 0, framesToWrite * stream->format->nBlockAlign);
    stream->renderClient->ReleaseBuffer(framesToWrite, 0);

    delayFrames -= framesToWrite;
  }

  // ==================== SYNCHRONIZATION BARRIER ====================
  // Signal that this thread is ready
  int ready = stream->readyCount->fetch_add(1) + 1;
  
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("Thread ready: ") + std::to_string(ready) + 
    "/" + std::to_string(stream->totalStreams->load()));
  
  // Wait for all threads to be ready
  {
    std::unique_lock<std::mutex> lock(*stream->startMutex);
    stream->startSignal->wait(lock, [&]() {
      return stream->readyCount->load() >= stream->totalStreams->load();
    });
  }
  
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("All threads synchronized, starting playback"));
  // =================================================================

  // NOW start audio client (all threads do this simultaneously)
  hr = stream->audioClient->Start();
  if(FAILED(hr))
  {
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
         std::string("Failed to start audio client"));
    stream->isPlaying = false;
    return;
  }

  stream->isPlaying = true;
  
  // Calculate samples per frame for source audio
  const uint32_t sourceSamplesPerFrame = audioData.channels;
  const uint32_t totalSourceFrames = static_cast<uint32_t>(audioData.samples.size()) / sourceSamplesPerFrame;
  
  const double sampleRateRatio = static_cast<double>(audioData.sampleRate) / 
                                  static_cast<double>(stream->format->nSamplesPerSec);
  const double playbackSpeed = speed * sampleRateRatio;
  
  double sourcePosition = 0.0;
  int iterationCount = 0;
  
  do
  {
    if(stream->shouldStop)
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Playback thread received stop signal"));
      break;
    }
    
    UINT32 numFramesPadding = 0;
    hr = stream->audioClient->GetCurrentPadding(&numFramesPadding);
    if(FAILED(hr))
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("GetCurrentPadding failed"));
      break;
    }
    
    UINT32 numFramesAvailable = stream->bufferFrameCount - numFramesPadding;
    
    if(numFramesAvailable > 0)
    {
      BYTE* pData = nullptr;
      hr = stream->renderClient->GetBuffer(numFramesAvailable, &pData);
      if(FAILED(hr))
      {
        Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
          std::string("GetBuffer failed"));
        break;
      }
      
      float* floatBuffer = reinterpret_cast<float*>(pData);
      const uint32_t outputChannels = stream->format->nChannels;
      
      for(UINT32 frame = 0; frame < numFramesAvailable; frame++)
      {
        uint32_t sourceFrame = static_cast<uint32_t>(sourcePosition);
        
        if(sourceFrame >= totalSourceFrames)
        {
          if(looping)
          {
            sourcePosition = 0.0;
            sourceFrame = 0;
          }
          else
          {
            for(UINT32 remainingFrame = frame; remainingFrame < numFramesAvailable; remainingFrame++)
            {
              for(uint32_t ch = 0; ch < outputChannels; ch++)
              {
                floatBuffer[remainingFrame * outputChannels + ch] = 0.0f;
              }
            }
            hr = stream->renderClient->ReleaseBuffer(numFramesAvailable, 0);
            goto exit_playback;
          }
        }
        
        double frac = sourcePosition - sourceFrame;
        uint32_t nextFrame = sourceFrame + 1;
        if(nextFrame >= totalSourceFrames)
          nextFrame = looping ? 0 : sourceFrame;
        
        float sourceSample = 0.0f;
        if(audioData.channels == 1)
        {
          float s1 = audioData.samples[sourceFrame];
          float s2 = audioData.samples[nextFrame];
          sourceSample = s1 + (s2 - s1) * static_cast<float>(frac);
        }
        else
        {
          float s1 = audioData.samples[sourceFrame * sourceSamplesPerFrame];
          float s2 = audioData.samples[nextFrame * sourceSamplesPerFrame];
          sourceSample = s1 + (s2 - s1) * static_cast<float>(frac);
        }
        
        sourceSample *= static_cast<float>(stream->volume);
        
        for(uint32_t ch = 0; ch < outputChannels; ch++)
        {
          if(stream->targetChannel < 0 || static_cast<uint32_t>(stream->targetChannel) == ch)
          {
            floatBuffer[frame * outputChannels + ch] = sourceSample;
          }
          else
          {
            floatBuffer[frame * outputChannels + ch] = 0.0f;
          }
        }
        
        sourcePosition += playbackSpeed;
      }
      
      hr = stream->renderClient->ReleaseBuffer(numFramesAvailable, 0);
      if(FAILED(hr))
      {
        Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
          std::string("ReleaseBuffer failed"));
        break;
      }
      
      iterationCount++;
    }
    
    Sleep(0);
    
  } while(!stream->shouldStop);
  
exit_playback:
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("Playback thread exiting, iterations: ") + std::to_string(iterationCount));
  
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
  
  auto audioIt = m_audioFiles.find(soundId);
  if(audioIt == m_audioFiles.end())
  {
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Audio file not loaded: ") + soundId);
    return false;
  }
  
  const AudioFileData& audioData = audioIt->second;
  
  if(m_activeSounds.count(soundId))
  {
    stopSound(soundId);
  }
  
  Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
    std::string("Starting playback: ") + soundId + 
    " on " + std::to_string(outputs.size()) + " output(s)");
  
  std::lock_guard<std::mutex> lock(m_impl->streamsMutex);
  auto& streams = m_impl->activeStreams[soundId];
  
  // Create shared synchronization objects for ALL streams
  auto startSignal = std::make_shared<std::condition_variable>();
  auto startMutex = std::make_shared<std::mutex>();
  auto readyCount = std::make_shared<std::atomic<int>>(0);
  auto totalStreams = std::make_shared<std::atomic<int>>(outputs.size());
  
  // Create all streams first
  for(const auto& config : outputs)
  {
    auto stream = std::make_unique<AudioStream>();
    
    stream->device = getDeviceById(m_impl->deviceEnumerator, config.deviceId);
    if(!stream->device)
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Failed to get device: ") + config.deviceId);
      continue;
    }
    
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
    
    hr = stream->audioClient->GetMixFormat(&stream->format);
    if(FAILED(hr))
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Failed to get mix format"));
      continue;
    }
    
    REFERENCE_TIME requestedDuration = 100000;
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
    
    hr = stream->audioClient->GetBufferSize(&stream->bufferFrameCount);
    if(FAILED(hr))
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Failed to get buffer size"));
      continue;
    }
    
    hr = stream->audioClient->GetService(
      __uuidof(IAudioRenderClient),
      (void**)&stream->renderClient);
    
    if(FAILED(hr))
    {
      Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
        std::string("Failed to get render client"));
      continue;
    }
    
    stream->targetChannel = config.channel;
    stream->volume = config.volume;
    stream->delaySeconds = config.delay / 1000.0;
    stream->shouldStop = false;
    
    // Share synchronization objects
    stream->startSignal = startSignal;
    stream->startMutex = startMutex;
    stream->readyCount = readyCount;
    stream->totalStreams = totalStreams;
    
    streams.push_back(std::move(stream));
  }
  
  if(streams.empty())
  {
    Log::log(std::string("WASAPIBackend"), LogMessage::I1006_X,
      std::string("Failed to create any audio streams"));
    m_impl->activeStreams.erase(soundId);
    return false;
  }
  
  // NOW start all threads (they will wait at the barrier)
  for(auto& stream : streams)
  {
    AudioStream* streamPtr = stream.get();
    stream->playbackThread = std::thread(playbackThreadFunc, streamPtr, 
                                         std::ref(audioData), looping, speed);
  }
  
  // Notify all waiting threads to start simultaneously
  {
    std::lock_guard<std::mutex> lock(*startMutex);
    startSignal->notify_all();
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

WASAPIAudioBackend::~WASAPIAudioBackend()
{
  // Stub implementation for non-Windows platforms
}

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
