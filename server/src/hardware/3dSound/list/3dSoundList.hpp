#pragma once
#include "../../core/objectlist.hpp"
#include "../3dSound.hpp"
#include <string>

class 3dSoundList : public ObjectList<3dSound>
{
public:
    Method<3dSoundList, std::shared_ptr<3dSound>> create;

    3dSoundList(Object& parent, std::string_view parentPropertyName);

    TableModelPtr getModel() override { return nullptr; } // no table model for now

    void worldEvent(WorldState state, WorldEvent event) override
    {
        ObjectList<3dSound>::worldEvent(state, event);
        const bool editable = contains(state, WorldState::Edit);
        Attributes::setEnabled(create, editable);
    }

protected:
    bool isListedProperty(std::string_view /*name*/) override
    {
        return false; // no properties listed for now
    }
};
