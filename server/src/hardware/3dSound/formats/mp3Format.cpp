#include "mp3Format.hpp"
#include <fstream>
#include <algorithm>
#include <vector>
#include <cstring>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include <minimp3_ex.hpp>


#pragma GCC diagnostic pop

bool MP3FormatLoader::canLoad(const std::string& filePath) const
{
  if(filePath.length() < 4)
    return false;
  
  std::string ext = filePath.substr(filePath.length() - 4);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  
  return ext == ".mp3";
}

bool MP3FormatLoader::load(const std::string& filePath, AudioFileData& outData,
                           std::string& outError) const
{
  mp3dec_t mp3d;
  mp3dec_file_info_t info;
  
  // Initialize decoder
  mp3dec_init(&mp3d);
  
  // Load entire MP3 file and decode
  int result = mp3dec_load(&mp3d, filePath.c_str(), &info, nullptr, nullptr);
  
  if(result != 0)
  {
    // Error codes from minimp3:
    // -1: file open error
    // -2: memory allocation error
    // -3: decode error
    switch(result)
    {
      case -1:
        outError = "Failed to open MP3 file: " + filePath;
        break;
      case -2:
        outError = "Memory allocation failed while loading MP3";
        break;
      case -3:
        outError = "Failed to decode MP3 file (corrupt or invalid format)";
        break;
      default:
        outError = "Unknown error loading MP3 file (code: " + std::to_string(result) + ")";
        break;
    }
    return false;
  }
  
  // Validate the decoded data
  if(info.samples == 0 || info.channels == 0 || info.hz == 0)
  {
    if(info.buffer)
      free(info.buffer);
    outError = "MP3 file contains no valid audio data";
    return false;
  }
  
  // Validate sample rate (8 kHz - 48 kHz for MP3)
  if(info.hz < 8000 || info.hz > 48000)
  {
    if(info.buffer)
      free(info.buffer);
    outError = "Unsupported MP3 sample rate: " + std::to_string(info.hz) + " Hz";
    return false;
  }
  
  // Validate channel count (MP3 supports 1-2 channels)
  if(info.channels == 0 || info.channels > 2)
  {
    if(info.buffer)
      free(info.buffer);
    outError = "Unsupported MP3 channel count: " + std::to_string(info.channels);
    return false;
  }
  
  // Copy data to our structure
  outData.sampleRate = info.hz;
  outData.channels = info.channels;
  outData.bitsPerSample = 16;  // MP3 is decoded to 16-bit equivalent
  
  // minimp3 with MINIMP3_FLOAT_OUTPUT gives us float samples directly
  size_t numSamples = info.samples;
  outData.samples.resize(numSamples);
  
  // Copy the decoded float samples
  std::memcpy(outData.samples.data(), info.buffer, numSamples * sizeof(float));
  
  // Free minimp3's buffer
  free(info.buffer);
  
  return true;
}
