#include "flacFormat.hpp"
#include <fstream>
#include <algorithm>
#include <vector>
#include <cstring>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-function"
#define DR_FLAC_IMPLEMENTATION
#include <dr_libs/dr_flac.hpp>
#pragma GCC diagnostic pop

bool FLACFormatLoader::canLoad(const std::string& filePath) const
{
  if(filePath.length() < 5)
    return false;
  
  std::string ext = filePath.substr(filePath.length() - 5);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  
  return ext == ".flac";
}

bool FLACFormatLoader::load(const std::string& filePath, AudioFileData& outData,
                            std::string& outError) const
{
  // Open FLAC file
  unsigned int channels = 0;
  unsigned int sampleRate = 0;
  drflac_uint64 totalPCMFrameCount = 0;
  
  // Decode entire FLAC file to 32-bit signed integer samples
  drflac_int32* pSampleData = drflac_open_file_and_read_pcm_frames_s32(
    filePath.c_str(),
    &channels,
    &sampleRate,
    &totalPCMFrameCount,
    nullptr
  );
  
  if(pSampleData == nullptr)
  {
    outError = "Failed to decode FLAC file (corrupt, invalid format, or file not found)";
    return false;
  }
  
  // Validate the decoded data
  if(totalPCMFrameCount == 0 || channels == 0 || sampleRate == 0)
  {
    drflac_free(pSampleData, nullptr);
    outError = "FLAC file contains no valid audio data";
    return false;
  }
  
  // Validate sample rate (8 kHz - 655 kHz for FLAC, but we'll limit to reasonable range)
  if(sampleRate < 8000 || sampleRate > 192000)
  {
    drflac_free(pSampleData, nullptr);
    outError = "Unsupported FLAC sample rate: " + std::to_string(sampleRate) + " Hz";
    return false;
  }
  
  // Validate channel count (FLAC supports 1-8 channels)
  if(channels == 0 || channels > 8)
  {
    drflac_free(pSampleData, nullptr);
    outError = "Unsupported FLAC channel count: " + std::to_string(channels);
    return false;
  }
  
  // Set output metadata
  outData.sampleRate = sampleRate;
  outData.channels = static_cast<uint16_t>(channels);
  outData.bitsPerSample = 16;  // We're converting to 16-bit equivalent float
  
  // Convert 32-bit signed int samples to normalized float [-1.0, 1.0]
  // dr_flac returns samples in the range appropriate for the original bit depth
  // but in 32-bit container, so we normalize by the full 32-bit range
  drflac_uint64 totalSampleCount = totalPCMFrameCount * channels;
  outData.samples.resize(totalSampleCount);
  
  for(drflac_uint64 i = 0; i < totalSampleCount; i++)
  {
    // Normalize from 32-bit signed integer to float
    outData.samples[i] = static_cast<float>(pSampleData[i]) / 2147483648.0f;
  }
  
  // Free dr_flac buffer
  drflac_free(pSampleData, nullptr);
  
  return true;
}
