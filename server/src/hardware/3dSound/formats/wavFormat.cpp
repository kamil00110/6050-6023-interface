#include "wavFormat.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cmath>

// WAV format codes
#define WAVE_FORMAT_PCM        0x0001  // PCM
#define WAVE_FORMAT_IEEE_FLOAT 0x0003  // IEEE float
#define WAVE_FORMAT_ALAW       0x0006  // 8-bit ITU-T G.711 A-law
#define WAVE_FORMAT_MULAW      0x0007  // 8-bit ITU-T G.711 µ-law
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE  // Extensible format

// GUID for PCM subformat (used in WAVE_FORMAT_EXTENSIBLE)
static const uint8_t KSDATAFORMAT_SUBTYPE_PCM[16] = {
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
  0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
};

// GUID for IEEE float subformat
static const uint8_t KSDATAFORMAT_SUBTYPE_IEEE_FLOAT[16] = {
  0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
  0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
};

struct WaveFormatExtensible
{
  uint16_t wFormatTag;
  uint16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  uint16_t nBlockAlign;
  uint16_t wBitsPerSample;
  uint16_t cbSize;           // Extra format bytes
  uint16_t wValidBitsPerSample;
  uint32_t dwChannelMask;
  uint8_t SubFormat[16];     // GUID
};

bool WAVFormatLoader::canLoad(const std::string& filePath) const
{
  // Check file extension first (fast path)
  if(filePath.length() >= 4)
  {
    std::string ext = filePath.substr(filePath.length() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if(ext == ".wav")
      return true;
  }
  
  // If no extension, check file header (magic bytes)
  std::ifstream file(filePath, std::ios::binary);
  if(!file.is_open())
    return false;
  
  char header[12];
  file.read(header, 12);
  if(file.gcount() < 12)
    return false;
  
  // Check for "RIFF....WAVE" or "RIFX....WAVE"
  return (std::memcmp(header, "RIFF", 4) == 0 || std::memcmp(header, "RIFX", 4) == 0) &&
         std::memcmp(header + 8, "WAVE", 4) == 0;
}

// Helper: Convert 8-bit unsigned to float
inline float uint8ToFloat(uint8_t sample)
{
  return (static_cast<float>(sample) - 128.0f) / 128.0f;
}

// Helper: Convert 16-bit signed to float
inline float int16ToFloat(int16_t sample)
{
  return static_cast<float>(sample) / 32768.0f;
}

// Helper: Convert 24-bit signed to float (stored in 32-bit int)
inline float int24ToFloat(int32_t sample)
{
  return static_cast<float>(sample) / 8388608.0f;
}

// Helper: Convert 32-bit signed to float
inline float int32ToFloat(int32_t sample)
{
  return static_cast<float>(sample) / 2147483648.0f;
}

// Helper: A-law decompression
inline float alawToFloat(uint8_t alaw)
{
  alaw ^= 0x55;
  int t = (alaw & 0x0f) << 4;
  int seg = (alaw & 0x70) >> 4;
  t = (t + 8) << (seg > 0 ? seg : 1);
  if(alaw & 0x80) t = -t;
  return static_cast<float>(t) / 32768.0f;
}

// Helper: µ-law decompression
inline float mulawToFloat(uint8_t mulaw)
{
  mulaw = ~mulaw;
  int sign = (mulaw & 0x80);
  int exponent = (mulaw >> 4) & 0x07;
  int mantissa = mulaw & 0x0F;
  int sample = ((mantissa << 3) + 0x84) << exponent;
  if(sign) sample = -sample;
  return static_cast<float>(sample) / 32768.0f;
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
    
    // Read RIFF/RIFX header
    char riffHeader[4];
    file.read(riffHeader, 4);
    bool isBigEndian __attribute__((unused)) = false;
    
    if(std::memcmp(riffHeader, "RIFF", 4) == 0)
    {
      isBigEndian = false;  // Little-endian
    }
    else if(std::memcmp(riffHeader, "RIFX", 4) == 0)
    {
      isBigEndian = true;   // Big-endian
    }
    else
    {
      outError = "Not a RIFF/RIFX file";
      return false;
    }
    
    uint32_t fileSize;
    file.read(reinterpret_cast<char*>(&fileSize), 4);
    
    char wave[4];
    file.read(wave, 4);
    if(std::memcmp(wave, "WAVE", 4) != 0)
    {
      outError = "Not a WAVE file";
      return false;
    }
    
    // Parse chunks
    uint16_t audioFormat = 0;
    uint16_t numChannels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    uint16_t validBitsPerSample = 0;
    uint32_t dataSize = 0;
    std::streampos dataPosition = 0;
    
    while(file.good())
    {
      char chunkId[4];
      file.read(chunkId, 4);
      if(!file.good()) break;
      
      uint32_t chunkSize;
      file.read(reinterpret_cast<char*>(&chunkSize), 4);
      
      if(std::memcmp(chunkId, "fmt ", 4) == 0)
      {
        std::streampos fmtStart = file.tellg();
        
        file.read(reinterpret_cast<char*>(&audioFormat), 2);
        file.read(reinterpret_cast<char*>(&numChannels), 2);
        file.read(reinterpret_cast<char*>(&sampleRate), 4);
        
        uint32_t byteRate;
        file.read(reinterpret_cast<char*>(&byteRate), 4);
        
        uint16_t blockAlign;
        file.read(reinterpret_cast<char*>(&blockAlign), 2);
        file.read(reinterpret_cast<char*>(&bitsPerSample), 2);
        
        validBitsPerSample = bitsPerSample;  // Default
        
        // Handle extensible format
        if(audioFormat == WAVE_FORMAT_EXTENSIBLE && chunkSize >= 40)
        {
          uint16_t cbSize;
          file.read(reinterpret_cast<char*>(&cbSize), 2);
          file.read(reinterpret_cast<char*>(&validBitsPerSample), 2);
          
          uint32_t channelMask;
          file.read(reinterpret_cast<char*>(&channelMask), 4);
          
          uint8_t subFormat[16];
          file.read(reinterpret_cast<char*>(subFormat), 16);
          
          // Check subformat GUID
          if(std::memcmp(subFormat, KSDATAFORMAT_SUBTYPE_PCM, 16) == 0)
          {
            audioFormat = WAVE_FORMAT_PCM;
          }
          else if(std::memcmp(subFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 16) == 0)
          {
            audioFormat = WAVE_FORMAT_IEEE_FLOAT;
          }
          else
          {
            outError = "Unsupported extensible subformat";
            return false;
          }
        }
        
        // Skip any remaining format bytes
        std::streampos currentPos = file.tellg();
        std::streampos expectedPos = fmtStart + static_cast<std::streamoff>(chunkSize);
        if(currentPos < expectedPos)
        {
          file.seekg(expectedPos);
        }
      }
      else if(std::memcmp(chunkId, "data", 4) == 0)
      {
        dataSize = chunkSize;
        dataPosition = file.tellg();
        break;  // Found data, stop searching
      }
      else
      {
        // Skip unknown chunk
        file.seekg(chunkSize, std::ios::cur);
      }
    }
    
    // Validate format
    if(audioFormat == 0 || numChannels == 0 || sampleRate == 0)
    {
      outError = "Invalid or missing format information";
      return false;
    }
    
    if(dataSize == 0)
    {
      outError = "No data chunk found";
      return false;
    }
    
    // Validate sample rates (support common rates)
    if(sampleRate < 8000 || sampleRate > 192000)
    {
      outError = "Sample rate out of range (8kHz - 192kHz): " + std::to_string(sampleRate);
      return false;
    }
    
    // Validate channel count
    if(numChannels == 0 || numChannels > 8)
    {
      outError = "Unsupported channel count: " + std::to_string(numChannels);
      return false;
    }
    
    // Seek to data
    file.seekg(dataPosition);
    
    // Set output metadata
    outData.sampleRate = sampleRate;
    outData.channels = numChannels;
    outData.bitsPerSample = bitsPerSample;
    
    // Calculate number of samples
    uint32_t bytesPerSample = bitsPerSample / 8;
    if(bytesPerSample == 0)
    {
      outError = "Invalid bits per sample: " + std::to_string(bitsPerSample);
      return false;
    }
    
    uint32_t numSamples = dataSize / bytesPerSample;
    outData.samples.resize(numSamples);
    
    // Read and convert to float based on format
    switch(audioFormat)
    {
      case WAVE_FORMAT_PCM:
      {
        if(bitsPerSample == 8)
        {
          // 8-bit unsigned
          std::vector<uint8_t> data(numSamples);
          file.read(reinterpret_cast<char*>(data.data()), dataSize);
          for(size_t i = 0; i < numSamples; i++)
          {
            outData.samples[i] = uint8ToFloat(data[i]);
          }
        }
        else if(bitsPerSample == 16)
        {
          // 16-bit signed
          std::vector<int16_t> data(numSamples);
          file.read(reinterpret_cast<char*>(data.data()), dataSize);
          for(size_t i = 0; i < numSamples; i++)
          {
            outData.samples[i] = int16ToFloat(data[i]);
          }
        }
        else if(bitsPerSample == 24)
        {
          // 24-bit signed (3 bytes per sample)
          for(size_t i = 0; i < numSamples; i++)
          {
            uint8_t bytes[3];
            file.read(reinterpret_cast<char*>(bytes), 3);
            
            // Convert 24-bit to 32-bit signed
            int32_t sample = (bytes[2] << 24) | (bytes[1] << 16) | (bytes[0] << 8);
            sample >>= 8;  // Sign-extend
            
            outData.samples[i] = int24ToFloat(sample);
          }
        }
        else if(bitsPerSample == 32)
        {
          // 32-bit signed
          std::vector<int32_t> data(numSamples);
          file.read(reinterpret_cast<char*>(data.data()), dataSize);
          for(size_t i = 0; i < numSamples; i++)
          {
            outData.samples[i] = int32ToFloat(data[i]);
          }
        }
        else
        {
          outError = "Unsupported PCM bit depth: " + std::to_string(bitsPerSample);
          return false;
        }
        break;
      }
      
      case WAVE_FORMAT_IEEE_FLOAT:
      {
        if(bitsPerSample == 32)
        {
          // 32-bit float - direct read
          file.read(reinterpret_cast<char*>(outData.samples.data()), dataSize);
        }
        else if(bitsPerSample == 64)
        {
          // 64-bit double - convert to float
          std::vector<double> data(numSamples);
          file.read(reinterpret_cast<char*>(data.data()), dataSize);
          for(size_t i = 0; i < numSamples; i++)
          {
            outData.samples[i] = static_cast<float>(data[i]);
          }
        }
        else
        {
          outError = "Unsupported float bit depth: " + std::to_string(bitsPerSample);
          return false;
        }
        break;
      }
      
      case WAVE_FORMAT_ALAW:
      {
        // 8-bit A-law
        std::vector<uint8_t> data(numSamples);
        file.read(reinterpret_cast<char*>(data.data()), dataSize);
        for(size_t i = 0; i < numSamples; i++)
        {
          outData.samples[i] = alawToFloat(data[i]);
        }
        break;
      }
      
      case WAVE_FORMAT_MULAW:
      {
        // 8-bit µ-law
        std::vector<uint8_t> data(numSamples);
        file.read(reinterpret_cast<char*>(data.data()), dataSize);
        for(size_t i = 0; i < numSamples; i++)
        {
          outData.samples[i] = mulawToFloat(data[i]);
        }
        break;
      }
      
      default:
        outError = "Unsupported audio format: 0x" + std::to_string(audioFormat);
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
