/**
 * server/src/hardware/3dSound/list/3dSoundList.hpp
 *
 * Container/list class for all 3D sounds.
 */

#pragma once

#include "../../core/objectlist.hpp"
#include "../3dSound.hpp"

class Sound3DList : public ObjectList<Sound3D>
{
public:
  Sound3DList(World& world, std::string_view _name)
    : ObjectList<Sound3D>(world, _name)
  {}
};

