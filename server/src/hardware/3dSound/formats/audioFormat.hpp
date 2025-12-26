#ifndef AUDIO_FORMAT_HPP
#define AUDIO_FORMAT_HPP

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

// Audio data structure (shared across all formats)
struct AudioFileData
{
  std::vector<float> samples;  // Normalized float samples
  uint32_t sampleRate;
  uint16_t channels;
  uint16_t bitsPerSample;
};

// Base class for audio format loaders
class IAudioFormatLoader
{
public:
  virtual ~IAudioFormatLoader() = default;
  
  // Check if this loader can handle the given file
  virtual bool canLoad(const std::string& filePath) const = 0;
  
  // Load audio file and return audio data
  virtual bool load(const std::string& filePath, AudioFileData& outData, 
                    std::string& outError) const = 0;
  
  // Get format name for logging
  virtual std::string getFormatName() const = 0;
};

// Factory for audio format loaders
class AudioFormatFactory
{
public:
  static AudioFormatFactory& instance();
  
  // Register a loader
  void registerLoader(std::unique_ptr<IAudioFormatLoader> loader);
  
  // Find appropriate loader for file
  IAudioFormatLoader* getLoader(const std::string& filePath) const;
  
  // Load audio file using appropriate loader
  bool loadAudioFile(const std::string& filePath, AudioFileData& outData,
                     std::string& outError) const;

private:
  AudioFormatFactory() = default;
  std::vector<std::unique_ptr<IAudioFormatLoader>> m_loaders;
};

#endif // AUDIO_FORMAT_HPP
