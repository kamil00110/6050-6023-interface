/**
 * server/src/hardware/3dSound/3dSound.cpp - CORRECTED VERSION
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
        // This validator is called when:
        // 1. Upload method sets the property (value is filename)
        // 2. World loading sets the property (value is filename)
        // 3. Someone explicitly sets it to empty (shouldn't happen in normal operation)
        
        // The validator should NOT delete files - that's handled by:
        // - destroying() when object is deleted
        // - uploadAudioFile() when replacing a file
        
        if(!value.empty() && value != m_originalFilename)
        {
          // Property is being set to a new filename
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Property validator: updating filename from '") + 
            m_originalFilename + "' to '" + value + "'");
          m_originalFilename = value;
        }
        else if(value.empty())
        {
          // Being cleared - just update the member variable
          // Don't delete the file here, as destroying() will handle it
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Property validator: property cleared"));
          m_originalFilename.clear();
        }
        
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
          // Log upload start with detailed info
          Log::log(*this, LogMessage::I1006_X, 
            std::string("=== UPLOAD STARTED ==="));
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Original filename: ") + filename);
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Data size: ") + std::to_string(data.size()) + " bytes");
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Object ID: ") + id.value());
          
          // Delete old file if it exists
          if(!m_originalFilename.empty())
          {
            Log::log(*this, LogMessage::I1006_X, 
              std::string("Deleting previous file: ") + m_originalFilename);
            deleteAudioFile();
          }
          
          // Get audio directory path
          const auto audioDir = getWorld(*this).audioFilesDir();
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Audio directory: ") + audioDir.string());
          
          // Check if directory exists, create if needed
          if(!std::filesystem::exists(audioDir))
          {
            Log::log(*this, LogMessage::I1006_X, 
              std::string("Creating audio directory..."));
          }
          
          // Generate filename based on object ID only (no extension)
          const std::string newFilename = id.value();
          const auto filePath = audioDir / newFilename;
          
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Target path: ") + filePath.string());
          
          // Write file (writeFile creates directories automatically)
          if(!writeFile(filePath, data.data(), data.size()))
          {
            Log::log(*this, LogMessage::I1006_X, 
              std::string("writeFile() returned false for: ") + filePath.string());
            throw std::runtime_error("Failed to write audio file");
          }
          
          // Verify file was created
          if(!std::filesystem::exists(filePath))
          {
            Log::log(*this, LogMessage::I1006_X, 
              std::string("File not found after write: ") + filePath.string());
            throw std::runtime_error("File verification failed");
          }
          
          const auto fileSize = std::filesystem::file_size(filePath);
          Log::log(*this, LogMessage::I1006_X, 
            std::string("File written successfully"));
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Verified size: ") + std::to_string(fileSize) + " bytes");
          
          // Update the property with ONLY the filename (not full path)
          // This will trigger the property validator, but won't delete because
          // it's setting a non-empty value
          m_originalFilename = newFilename;
          soundFile.setValueInternal(newFilename);
          
          Log::log(*this, LogMessage::I1006_X, 
            std::string("=== UPLOAD COMPLETE: ") + newFilename + " ===");
        }
        catch(const std::exception& e)
        {
          Log::log(*this, LogMessage::I1006_X, 
            std::string("Upload failed with exception: ") + e.what());
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
  Attributes::addStep(volume, 0.1);
  Attributes::addEnabled(volume, true);
  m_interfaceItems.add(volume);
  
  Attributes::addDisplayName(speed, "Speed");
  Attributes::addMinMax(speed, 0.1, 3.0);
  Attributes::addStep(speed, 0.1);
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
  // Delete the audio file when this object is being destroyed
  Log::log(*this, LogMessage::I1006_X, 
    std::string("Object destroying, cleaning up audio file"));
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
  {
    Log::log(*this, LogMessage::I1006_X, 
      std::string("deleteAudioFile: No filename to delete"));
    return;
  }
  
  // Store filename locally in case m_originalFilename gets cleared during deletion
  const std::string filenameToDelete = m_originalFilename;
  
  try
  {
    // Construct full path to the audio file
    const auto audioDir = getWorld(*this).audioFilesDir();
    const auto filePath = audioDir / filenameToDelete;
    
    Log::log(*this, LogMessage::I1006_X, 
      std::string("Deleting audio file: ") + filePath.string());
    
    if(std::filesystem::exists(filePath))
    {
      std::filesystem::remove(filePath);
      Log::log(*this, LogMessage::I1006_X, 
        std::string("Successfully deleted audio file: ") + filenameToDelete);
    }
    else
    {
      Log::log(*this, LogMessage::I1006_X, 
        std::string("Audio file not found (may have been deleted already): ") + filePath.string());
    }
    
    // Clear the member variable after successful deletion (or if file didn't exist)
    m_originalFilename.clear();
  }
  catch(const std::exception& e)
  {
    Log::log(*this, LogMessage::I1006_X, 
      std::string("Failed to delete audio file: ") + e.what());
    // Still clear the member variable even if deletion failed
    m_originalFilename.clear();
  }
}

std::filesystem::path ThreeDSound::getAudioFilePath()
{
  // This constructs: <worldDir>/<uuid>/audio/<filename>
  const auto audioDir = getWorld(*this).audioFilesDir();
  const auto fullPath = audioDir / m_originalFilename;
  
  Log::log(*this, LogMessage::I1006_X, 
    std::string("getAudioFilePath: ") + fullPath.string());
  
  return fullPath;
}
