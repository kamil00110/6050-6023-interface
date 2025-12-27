#include "w8vFormat.hpp"
#include "wavFormat.hpp"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <vector>

// W8V format signature
static const char W8V_SIGNATURE[] = "WinDigital 8 Sound File";
static const size_t W8V_SIGNATURE_LENGTH = 23; // Without null terminator

bool W8VFormatLoader::canLoad(const std::string& filePath) const
{
  // Check file extension first (fast path)
  if(filePath.length() >= 4)
  {
    std::string ext = filePath.substr(filePath.length() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if(ext == ".w8v")
      return true;
  }
  
  // If no extension, check file header (magic bytes)
  std::ifstream file(filePath, std::ios::binary);
  if(!file.is_open())
    return false;
  
  char signature[W8V_SIGNATURE_LENGTH];
  file.read(signature, W8V_SIGNATURE_LENGTH);
  if(file.gcount() < static_cast<std::streamsize>(W8V_SIGNATURE_LENGTH))
    return false;
  
  // Check for "WinDigital 8 Sound File" signature
  return std::memcmp(signature, W8V_SIGNATURE, W8V_SIGNATURE_LENGTH) == 0;
}

bool W8VFormatLoader::load(const std::string& filePath, AudioFileData& outData,
                           std::string& outError) const
{
  try
  {
    std::ifstream file(filePath, std::ios::binary);
    if(!file.is_open())
    {
      outError = "Failed to open W8V file: " + filePath;
      return false;
    }
    
    // Read and verify W8V signature
    char signature[W8V_SIGNATURE_LENGTH];
    file.read(signature, W8V_SIGNATURE_LENGTH);
    
    if(!file.good() || std::memcmp(signature, W8V_SIGNATURE, W8V_SIGNATURE_LENGTH) != 0)
    {
      outError = "Not a valid W8V file (missing WinDigital 8 Sound File signature)";
      return false;
    }
    
    // Search for RIFF or IFF header within the next 1024 bytes
    // (The W8V wrapper appears to have variable-length metadata)
    // Note: Some W8V files use "IFF" instead of "RIFF"
    std::vector<char> searchBuffer(1024);
    std::streampos startPos = file.tellg();
    file.read(searchBuffer.data(), searchBuffer.size());
    std::streamsize bytesRead = file.gcount();
    
    // Look for "RIFF" or "IFF" signature followed by "WAVE"
    std::streampos wavOffset = -1;
    bool needsRiffPrefix = false;
    
    for(std::streamsize i = 0; i < bytesRead - 8; i++)
    {
      // Check for standard RIFF header
      if(std::memcmp(&searchBuffer[i], "RIFF", 4) == 0)
      {
        wavOffset = startPos + static_cast<std::streamoff>(i);
        needsRiffPrefix = false;
        break;
      }
      // Check for IFF header (W8V variant) - needs 'R' prefix to make it RIFF
      else if(std::memcmp(&searchBuffer[i], "IFF", 3) == 0)
      {
        // Verify it's followed by size + "WAVE"
        if(i + 11 < bytesRead && std::memcmp(&searchBuffer[i + 7], "WAVE", 4) == 0)
        {
          wavOffset = startPos + static_cast<std::streamoff>(i);
          needsRiffPrefix = true;
          break;
        }
      }
    }
    
    if(wavOffset == -1)
    {
      outError = "Could not find RIFF/IFF header in W8V file (corrupt or unsupported variant)";
      return false;
    }
    
    // Seek to RIFF/IFF header
    file.seekg(wavOffset);
    
    // Calculate WAV data size
    file.seekg(0, std::ios::end);
    std::streampos fileEnd = file.tellg();
    std::streamsize wavSize = fileEnd - wavOffset;
    
    // If we found IFF instead of RIFF, we need to add 'R' prefix
    if(needsRiffPrefix)
    {
      wavSize += 1; // Account for the 'R' we'll add
    }
    
    file.seekg(wavOffset);
    std::vector<char> wavData(wavSize);
    
    if(needsRiffPrefix)
    {
      // Add 'R' prefix to make "IFF" into "RIFF"
      wavData[0] = 'R';
      file.read(wavData.data() + 1, wavSize - 1);
    }
    else
    {
      // Standard RIFF - just read it
      file.read(wavData.data(), wavSize);
    }
    
    if(!file.good())
    {
      outError = "Failed to read WAV data from W8V file";
      return false;
    }
    
    // Write temporary WAV file for parsing
    // (Alternative: modify WAV loader to accept memory buffer)
    std::string tempPath = filePath + ".tmp.wav";
    std::ofstream tempFile(tempPath, std::ios::binary);
    if(!tempFile.is_open())
    {
      outError = "Failed to create temporary WAV file";
      return false;
    }
    
    tempFile.write(wavData.data(), wavSize);
    tempFile.close();
    
    // Use WAV loader
    WAVFormatLoader wavLoader;
    bool result = wavLoader.load(tempPath, outData, outError);
    
    // Clean up temporary file
    std::remove(tempPath.c_str());
    
    if(!result)
    {
      outError = "Failed to parse WAV data in W8V file: " + outError;
      return false;
    }
    
    return true;
  }
  catch(const std::exception& e)
  {
    outError = std::string("Exception loading W8V file: ") + e.what();
    return false;
  }
}
