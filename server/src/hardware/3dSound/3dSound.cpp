/**
 * server/src/hardware/3dSound/3dSound.cpp - COMPLETE FIXED VERSION
 */

#include "3dSound.hpp"
#include "list/3dSoundList.hpp"
#include "list/3dSoundListTableModel.hpp"
#include "../../world/getworld.hpp"
#include "../../world/world.hpp"
#include "../../core/attributes.hpp"
#include "../../core/method.tpp"
#include "../../core/objectproperty.tpp"
#include "../../utils/displayname.hpp"
#include "../../utils/writefile.hpp"
#include "../../log/log.hpp"

ThreeDSound::ThreeDSound(World& world, std::string_view _id)
  : IdObject(world, _id)
  , soundFile{this, "sound_file", "", PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::string& value)
      {
        // IMPORTANT: Only delete if the value is different AND not empty
        // Empty value means user is clearing the file
        if(!m_originalFilename.empty() && m_originalFilename != value && !value.empty())
        {
          deleteAudioFile();
        }
        
        // If value is empty, user is clearing the file
        if(value.empty() && !m_originalFilename.empty())
        {
          deleteAudioFile();
        }
        
        m_originalFilename = value;
        return true;
      }}
  , looping{this, "looping", false, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , volume{this, "volume", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , speed{this, "speed", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , uploadAudioFile{*this, "upload_audio_file",
      [this](const std::string& filename, const std::string& data)
      {
        try
        {
          // Log upload start
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Upload started: ") + filename + " (" + std::to_string(data.size()) + " bytes)");
          
          // Delete old file if exists
          if(!m_originalFilename.empty())
          {
            Log::log(*this, LogMessage::I1006_X, 
              std::string("Deleting old file: ") + m_originalFilename);
            deleteAudioFile();
          }
          
          // Get audio directory path
          const auto audioDir = getWorld(*this).audioFilesDir();
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Audio directory: ") + audioDir.string());
          
          // Generate unique filename based on object ID and original extension
          const std::filesystem::path originalPath(filename);
          const std::string extension = originalPath.extension().string();
          const std::string newFilename = id.value() + extension;
          const auto filePath = audioDir / newFilename;
          
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Writing to: ") + filePath.string());
          
          // Write file using utility function (creates directories automatically)
          if(!writeFile(filePath, data.data(), data.size()))
          {
            Log::log(*this, LogMessage::E1003_X, std::string("writeFile returned false"));
            throw std::runtime_error("Failed to write audio file");
          }
          
          // Verify file was written
          if(!std::filesystem::exists(filePath))
          {
            Log::log(*this, LogMessage::I1006_X, std::string("File does not exist after write"));
            throw std::runtime_error("File verification failed");
          }
          
          const auto fileSize = std::filesystem::file_size(filePath);
          Log::log(*this, LogMessage::I1006_X, 
            std::string("File written successfully, size: ") + std::to_string(fileSize));
          
          // Update property with ONLY the filename (not full path)
          m_originalFilename = newFilename;
          soundFile.setValueInternal(newFilename);
          
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Upload complete: ") + newFilename);
        }
        catch(const std::exception& e)
        {
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Upload failed: ") + e.what());
          throw;
        }
      }}
{
  Attributes::addDisplayName(soundFile, "File");
  Attributes::addEnabled(soundFile, true);
  m_interfaceItems.add(soundFile);

  // Hide upload method from UI
  Attributes::addObjectEditor(uploadAudioFile, false);
  m_interfaceItems.add(uploadAudioFile);
  
  Attributes::addDisplayName(looping, "Loop");
  Attributes::addEnabled(looping, true);
  m_interfaceItems.add(looping);
  
  Attributes::addDisplayName(volume, "Volume");
  Attributes::addMinMax(volume, 0.0, 1.0);
  Attributes::addEnabled(volume, true);
  m_interfaceItems.add(volume);
  
  Attributes::addDisplayName(speed, "Speed");
  Attributes::addMinMax(speed, 0.1, 3.0);
  Attributes::addEnabled(speed, true);
  m_interfaceItems.add(speed);
  
  updateEnabled();
}

void ThreeDSound::addToWorld()
{
  IdObject::addToWorld();
  if(auto list = getWorld(*this).threeDSounds.value())
    list->addObject(shared_ptr<ThreeDSound>());
}

void ThreeDSound::loaded()
{
  IdObject::loaded();
  updateEnabled();
}

void ThreeDSound::destroying()
{
  deleteAudioFile();
  
  if(auto list = getWorld(*this).threeDSounds.value())
    list->removeObject(shared_ptr<ThreeDSound>());
  IdObject::destroying();
}

void ThreeDSound::worldEvent(WorldState state, WorldEvent event)
{
  IdObject::worldEvent(state, event);
  updateEnabled();
}

void ThreeDSound::updateEnabled()
{
  const bool editable = contains(getWorld(*this).state.value(), WorldState::Edit);
  Attributes::setEnabled(soundFile, editable);
  Attributes::setEnabled(looping, editable);
  Attributes::setEnabled(volume, editable);
  Attributes::setEnabled(speed, editable);
}

void ThreeDSound::deleteAudioFile()
{
  if(m_originalFilename.empty())
    return;
    
  try
  {
    const auto filePath = getAudioFilePath();
    
    Log::log(*this, LogMessage::I1006_X, 
      std::string("Attempting to delete: ") + filePath.string());
    
    if(std::filesystem::exists(filePath))
    {
      std::filesystem::remove(filePath);
      Log::log(*this, LogMessage::I1006_X, 
        std::string("Deleted: ") + m_originalFilename);
    }
    else
    {
      Log::log(*this, LogMessage::I1006_X, 
        std::string("File does not exist: ") + filePath.string());
    }
  }
  catch(const std::exception& e)
  {
    Log::log(*this, LogMessage::I1006_X, 
      std::string("Delete failed: ") + e.what());
  }
}

std::filesystem::path ThreeDSound::getAudioFilePath()
{
  return getWorld(*this).audioFilesDir() / m_originalFilename;
}
