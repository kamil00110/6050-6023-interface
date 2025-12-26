#include "wavFormat.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>

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

bool WAVFormatLoader::canLoad(const std::string& filePath) const
{
  // Check file extension
  if(filePath.length() < 4)
    return false;
  
  std::string ext = filePath.substr(filePath.length() - 4);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  
  return ext == ".wav";
}

bool WAVFormatLoader::load(const std::string& filePath, AudioFileData& outData,
                           std::string& outError) const
{
  try
  {
    std::ifstream file(filePath, std::ios::binary);
    if(!file.is_open())
    {
      outError = "Failed to open file: " + filePath;
      return false;
    }
    
    // Read RIFF header
    char riff[4];
    file.read(riff, 4);
    if(std::memcmp(riff, "RIFF", 4) != 0)
    {
      outError = "Not a RIFF file: " + filePath;
      return false;
    }
    
    uint32_t fileSize;
    file.read(reinterpret_cast<char*>(&fileSize), 4);
    
    char wave[4];
    file.read(wave, 4);
    if(std::memcmp(wave, "WAVE", 4) != 0)
    {
      outError = "Not a WAVE file: " + filePath;
      return false;
    }
    
    // Find fmt chunk
    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    
    while(file.good())
    {
      char chunkId[4];
      file.read(chunkId, 4);
      if(!file.good()) break;
      
      uint32_t chunkSize;
      file.read(reinterpret_cast<char*>(&chunkSize), 4);
      
      if(std::memcmp(chunkId, "fmt ", 4) == 0)
      {
        file.read(reinterpret_cast<char*>(&audioFormat), 2);
        file.read(reinterpret_cast<char*>(&numChannels), 2);
        file.read(reinterpret_cast<char*>(&sampleRate), 4);
        
        uint32_t byteRate;
        file.read(reinterpret_cast<char*>(&byteRate), 4);
        
        uint16_t blockAlign;
        file.read(reinterpret_cast<char*>(&blockAlign), 2);
        file.read(reinterpret_cast<char*>(&bitsPerSample), 2);
        
        // Skip any extra format bytes
        if(chunkSize > 16)
        {
          file.seekg(chunkSize - 16, std::ios::cur);
        }
        break;
      }
      else
      {
        // Skip this chunk
        file.seekg(chunkSize, std::ios::cur);
      }
    }
    
    if(audioFormat != 1)
    {
      outError = "Unsupported audio format (must be PCM): " + std::to_string(audioFormat);
      return false;
    }
    
    // Find data chunk
    uint32_t dataSize = 0;
    while(file.good())
    {
      char chunkId[4];
      file.read(chunkId, 4);
      if(!file.good()) break;
      
      uint32_t chunkSize;
      file.read(reinterpret_cast<char*>(&chunkSize), 4);
      
      if(std::memcmp(chunkId, "data", 4) == 0)
      {
        dataSize = chunkSize;
        break;
      }
      else
      {
        // Skip this chunk
        file.seekg(chunkSize, std::ios::cur);
      }
    }
    
    if(dataSize == 0)
    {
      outError = "No data chunk found in WAV file: " + filePath;
      return false;
    }
    
    // Read audio data
    outData.sampleRate = sampleRate;
    outData.channels = numChannels;
    outData.bitsPerSample = bitsPerSample;
    
    // Calculate number of samples
    uint32_t numSamples = dataSize / (bitsPerSample / 8);
    outData.samples.resize(numSamples);
    
    // Read and convert to float
    if(bitsPerSample == 16)
    {
      std::vector<int16_t> int16Data(numSamples);
      file.read(reinterpret_cast<char*>(int16Data.data()), dataSize);
      
      for(size_t i = 0; i < numSamples; i++)
      {
        outData.samples[i] = int16Data[i] / 32768.0f;
      }
    }
    else if(bitsPerSample == 32)
    {
      file.read(reinterpret_cast<char*>(outData.samples.data()), dataSize);
    }
    else
    {
      outError = "Unsupported bit depth: " + std::to_string(bitsPerSample);
      return false;
    }
    
    return true;
  }
  catch(const std::exception& e)
  {
    outError = std::string("Exception: ") + e.what();
    return false;
  }
}
