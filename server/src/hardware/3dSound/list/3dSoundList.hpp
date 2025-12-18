#pragma once

#include "../../core/objectlist.hpp"
#include "../3dSound.hpp"

class Sound3DList : public ObjectList<Sound3D>
{
public:
    Sound3DList(Object& parent, std::string_view parentPropertyName);

    Method create;
    Method delete_;

    TableModelPtr getModel() override;
    void worldEvent(WorldState state, WorldEvent event) override;
    bool isListedProperty(std::string_view name) override;
};
