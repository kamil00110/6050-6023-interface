#ifndef FLAC_FORMAT_HPP
#define FLAC_FORMAT_HPP

#include "audioFormat.hpp"

class FLACFormatLoader : public IAudioFormatLoader
{
public:
  bool canLoad(const std::string& filePath) const override;
  bool load(const std::string& filePath, AudioFileData& outData, 
            std::string& outError) const override;
  std::string getFormatName() const override { return "FLAC"; }
};

#endif // FLAC_FORMAT_HPP
