#pragma once
#include "../3dSound.hpp"
#include "../../../core/objectlist.hpp"
#include "../../../world/getworld.hpp"
#include "../../../core/attributes.hpp"

class 3dSoundList : public ObjectList<3dSound>
{
public:
    3dSoundList(Object& parent, std::string_view parentPropertyName)
        : ObjectList<3dSound>(parent, parentPropertyName)
    {
        const bool editable = contains(getWorld(parent).state.value(), WorldState::Edit);

        // Simple "create" method
        create = Method<3dSoundList, std::shared_ptr<3dSound>>(
            *this, "create", [this, &parent]() {
                auto& world = getWorld(parent);
                auto sound = std::make_shared<3dSound>(world, world.getUniqueId("3dSound"));
                addObject(sound);
                return sound;
            });

        Attributes::addDisplayName(create, "Create 3D Sound");
        Attributes::addEnabled(create, editable);
    }

    void worldEvent(WorldState state, WorldEvent event)
    {
        ObjectList<3dSound>::worldEvent(state, event);
        const bool editable = contains(state, WorldState::Edit);
        Attributes::setEnabled(create, editable);
    }

    bool isListedProperty(std::string_view name) override
    {
        // You can list properties you care about for the ObjectList here
        return name == "name"; // Example
    }

private:
    Method<3dSoundList, std::shared_ptr<3dSound>> create;
};
