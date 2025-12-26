#include "audioFormat.hpp"
#include <algorithm>

AudioFormatFactory& AudioFormatFactory::instance()
{
  static AudioFormatFactory factory;
  return factory;
}

void AudioFormatFactory::registerLoader(std::unique_ptr<IAudioFormatLoader> loader)
{
  if(loader)
  {
    m_loaders.push_back(std::move(loader));
  }
}

IAudioFormatLoader* AudioFormatFactory::getLoader(const std::string& filePath) const
{
  for(const auto& loader : m_loaders)
  {
    if(loader->canLoad(filePath))
    {
      return loader.get();
    }
  }
  return nullptr;
}

bool AudioFormatFactory::loadAudioFile(const std::string& filePath, 
                                       AudioFileData& outData,
                                       std::string& outError) const
{
  IAudioFormatLoader* loader = getLoader(filePath);
  if(!loader)
  {
    outError = "No loader found for file: " + filePath;
    return false;
  }
  
  return loader->load(filePath, outData, outError);
}
