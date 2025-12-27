#ifndef W8V_FORMAT_HPP
#define W8V_FORMAT_HPP

#include "audioFormat.hpp"

class W8VFormatLoader : public IAudioFormatLoader
{
public:
  bool canLoad(const std::string& filePath) const override;
  bool load(const std::string& filePath, AudioFileData& outData, 
            std::string& outError) const override;
  std::string getFormatName() const override { return "W8V"; }
};

#endif // W8V_FORMAT_HPP
