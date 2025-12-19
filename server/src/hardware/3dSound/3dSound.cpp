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
#include "../../log/log.hpp"

ThreeDSound::ThreeDSound(World& world, std::string_view _id)
  : IdObject(world, _id)
  , name{this, "name", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , positionX{this, "position_x", 0.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , positionY{this, "position_y", 0.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , positionZ{this, "position_z", 0.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , volume{this, "volume", 1.0, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , soundFile{this, "sound_file", "", PropertyFlags::ReadWrite | PropertyFlags::Store}
  , looping{this, "looping", false, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , isPlaying{this, "is_playing", false, PropertyFlags::ReadOnly | PropertyFlags::NoStore}
  , play{*this, "play",
      [this]()
      {
        if(!soundFile.value().empty())
        {
          isPlaying.setValueInternal(true);
          // TODO: Implement actual audio playback
        }
      }}
  , stop{*this, "stop",
      [this]()
      {
        isPlaying.setValueInternal(false);
        // TODO: Implement actual audio stop
      }}
  , test{*this, "test",
      [this]()
      {
        if(!soundFile.value().empty())
        {
          // TODO: Play a short test sound
        }
      }}
{
  Attributes::addDisplayName(name, DisplayName::Object::name);
  Attributes::addEnabled(name, true);
  m_interfaceItems.add(name);

  Attributes::addDisplayName(positionX, "Position X");
  Attributes::addMinMax(positionX, -1000.0, 1000.0);
  Attributes::addEnabled(positionX, true);
  m_interfaceItems.add(positionX);

  Attributes::addDisplayName(positionY, "Position Y");
  Attributes::addMinMax(positionY, -1000.0, 1000.0);
  Attributes::addEnabled(positionY, true);
  m_interfaceItems.add(positionY);

  Attributes::addDisplayName(positionZ, "Position Z");
  Attributes::addMinMax(positionZ, -1000.0, 1000.0);
  Attributes::addEnabled(positionZ, true);
  m_interfaceItems.add(positionZ);

  Attributes::addDisplayName(volume, "Volume");
  Attributes::addMinMax(volume, 0.0, 1.0);
  Attributes::addEnabled(volume, true);
  m_interfaceItems.add(volume);

  Attributes::addDisplayName(soundFile, "Sound File");
  Attributes::addEnabled(soundFile, true);
  m_interfaceItems.add(soundFile);

  Attributes::addDisplayName(looping, "Looping");
  Attributes::addEnabled(looping, true);
  m_interfaceItems.add(looping);

  Attributes::addDisplayName(isPlaying, "Is Playing");
  m_interfaceItems.add(isPlaying);

  Attributes::addDisplayName(play, "Play");
  Attributes::addEnabled(play, false);
  m_interfaceItems.add(play);

  Attributes::addDisplayName(stop, "Stop");
  Attributes::addEnabled(stop, false);
  m_interfaceItems.add(stop);

  Attributes::addDisplayName(test, "Test");
  Attributes::addEnabled(test, false);
  m_interfaceItems.add(test);

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
  const bool hasFile = !soundFile.value().empty();

  Attributes::setEnabled(name, editable);
  Attributes::setEnabled(positionX, editable);
  Attributes::setEnabled(positionY, editable);
  Attributes::setEnabled(positionZ, editable);
  Attributes::setEnabled(volume, editable);
  Attributes::setEnabled(soundFile, editable);
  Attributes::setEnabled(looping, editable);
  
  Attributes::setEnabled(play, hasFile && !isPlaying.value());
  Attributes::setEnabled(stop, isPlaying.value());
  Attributes::setEnabled(test, hasFile);
}
