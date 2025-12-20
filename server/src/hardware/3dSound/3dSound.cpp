/**
 * server/src/hardware/3dSound/3dSound.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
        if(!m_originalFilename.empty() && m_originalFilename != value)
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
          // Delete old file if exists
          if(!m_originalFilename.empty())
          {
            deleteAudioFile();
          }
          
          // Get audio directory path
          const auto audioDir = getWorld(*this).audioFilesDir();
          
          // Generate unique filename
          const std::filesystem::path originalPath(filename);
          const std::string extension = originalPath.extension().string();
          const std::string newFilename = id.value() + extension;
          const auto filePath = audioDir / newFilename;
          
          // Write file - data.data() points to the raw bytes
          if(!writeFile(filePath, data.data(), data.size()))
          {
            throw std::runtime_error("Failed to write audio file");
          }
          
          // Update property
          m_originalFilename = newFilename;
          soundFile.setValueInternal(newFilename);
          
          Log::log(*this, LogMessage::N1001_X, 
            "Audio file uploaded: " + filename + " (" + std::to_string(data.size()) + " bytes)");
        }
        catch(const std::exception& e)
        {
          Log::log(*this, LogMessage::E1001_X, 
            std::string("Failed to upload audio: ") + e.what());
          throw;
        }
      }}
{
  Attributes::addDisplayName(soundFile, "File");
  Attributes::addEnabled(soundFile, true);
  m_interfaceItems.add(soundFile);

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
    if(std::filesystem::exists(filePath))
    {
      std::filesystem::remove(filePath);
      Log::log(*this, LogMessage::N1001_X, 
        "Audio file deleted: " + m_originalFilename);
    }
  }
  catch(const std::exception& e)
  {
    Log::log(*this, LogMessage::W1001_X, 
      std::string("Failed to delete audio: ") + e.what());
  }
}

std::filesystem::path ThreeDSound::getAudioFilePath()
{
  return getWorld(*this).audioFilesDir() / m_originalFilename;
}
