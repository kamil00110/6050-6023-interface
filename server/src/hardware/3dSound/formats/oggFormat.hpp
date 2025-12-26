#ifndef OGG_FORMAT_HPP
#define OGG_FORMAT_HPP

#include "audioFormat.hpp"

class OGGFormatLoader : public IAudioFormatLoader
{
public:
  bool canLoad(const std::string& filePath) const override;
  bool load(const std::string& filePath, AudioFileData& outData, 
            std::string& outError) const override;
  std::string getFormatName() const override { return "OGG"; }
};

#endif // OGG_FORMAT_HPP
