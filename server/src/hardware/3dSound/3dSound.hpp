/**
 * server/src/hardware/3dSound/3dSound.hpp
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

#ifndef TRAINTASTIC_SERVER_HARDWARE_3DSOUND_3DSOUND_HPP
#define TRAINTASTIC_SERVER_HARDWARE_3DSOUND_3DSOUND_HPP

#include "../../core/idobject.hpp"
#include "../../core/property.hpp"
#include "../../core/method.hpp"

class ThreeDSound : public IdObject
{
    CLASS_ID("3d_sound")
    DEFAULT_ID("sound")
    CREATE(ThreeDSound)

private:
    void updateEnabled();
    bool importSoundFile(std::string& value);

protected:
    void addToWorld() override;
    void loaded() override;
    void destroying() override;
    void worldEvent(WorldState state, WorldEvent event) override;

public:
    Property<std::string> soundFile;  // File
    Property<bool> looping;           // Loop
    Property<double> volume;          // Volume
    Property<double> speed;           // Speed

    ThreeDSound(World& world, std::string_view _id);
};

#endif
