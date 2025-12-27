#include "oggFormat.hpp"
#include <fstream>
#include <algorithm>
#include <vector>
#include <cstring>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_VORBIS_NO_PUSHDATA_API
#include <stb/stb_vorbis.cpp>
#pragma GCC diagnostic pop

bool OGGFormatLoader::canLoad(const std::string& filePath) const
{
  if(filePath.length() < 4)
    return false;
  
  std::string ext = filePath.substr(filePath.length() - 4);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  
  return ext == ".ogg";
}

bool OGGFormatLoader::load(const std::string& filePath, AudioFileData& outData,
                           std::string& outError) const
{
  // Open file and read into memory
  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if(!file.is_open())
  {
    outError = "Failed to open OGG file: " + filePath;
    return false;
  }
  
  std::streamsize fileSize = file.tellg();
  file.seekg(0, std::ios::beg);
  
  if(fileSize <= 0)
  {
    outError = "OGG file is empty or invalid";
    return false;
  }
  
  std::vector<uint8_t> fileData(fileSize);
  if(!file.read(reinterpret_cast<char*>(fileData.data()), fileSize))
  {
    outError = "Failed to read OGG file data";
    return false;
  }
  
  // Decode OGG Vorbis using stb_vorbis
  int channels = 0;
  int sampleRate = 0;
  short* decoded = nullptr;
  int numSamples = stb_vorbis_decode_memory(
    fileData.data(),
    static_cast<int>(fileSize),
    &channels,
    &sampleRate,
    &decoded
  );
  
  if(numSamples < 0 || decoded == nullptr)
  {
    outError = "Failed to decode OGG Vorbis file (corrupt or invalid format)";
    return false;
  }
  
  // Validate the decoded data
  if(numSamples == 0 || channels == 0 || sampleRate == 0)
  {
    if(decoded)
      free(decoded);
    outError = "OGG file contains no valid audio data";
    return false;
  }
  
  // Validate sample rate (8 kHz - 192 kHz for Vorbis)
  if(sampleRate < 8000 || sampleRate > 192000)
  {
    free(decoded);
    outError = "Unsupported OGG sample rate: " + std::to_string(sampleRate) + " Hz";
    return false;
  }
  
  // Validate channel count (Vorbis supports 1-8 channels)
  if(channels == 0 || channels > 8)
  {
    free(decoded);
    outError = "Unsupported OGG channel count: " + std::to_string(channels);
    return false;
  }
  
  // Set output metadata
  outData.sampleRate = static_cast<uint32_t>(sampleRate);
  outData.channels = static_cast<uint16_t>(channels);
  outData.bitsPerSample = 16;  // stb_vorbis decodes to 16-bit
  
  // Convert 16-bit signed int samples to normalized float [-1.0, 1.0]
  size_t totalSamples = static_cast<size_t>(numSamples) * channels;
  outData.samples.resize(totalSamples);
  
  for(size_t i = 0; i < totalSamples; i++)
  {
    outData.samples[i] = static_cast<float>(decoded[i]) / 32768.0f;
  }
  
  // Free stb_vorbis buffer
  free(decoded);
  
  return true;
}
