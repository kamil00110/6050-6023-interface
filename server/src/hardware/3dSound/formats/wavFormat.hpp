#ifndef WAV_FORMAT_HPP
#define WAV_FORMAT_HPP

#include "audioFormat.hpp"

class WAVFormatLoader : public IAudioFormatLoader
{
public:
  bool canLoad(const std::string& filePath) const override;
  bool load(const std::string& filePath, AudioFileData& outData, 
            std::string& outError) const override;
  std::string getFormatName() const override { return "WAV"; }
};

#endif // WAV_FORMAT_HPP
