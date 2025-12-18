#include "3dSoundList.hpp"
#include "3dSoundListTableModel.hpp"
#include "../../world/getworld.hpp"
#include "../../core/attributes.hpp"

3dSoundList::3dSoundList(Object& parent, std::string_view parentPropertyName)
    : ObjectList<3dSound>(parent, parentPropertyName)
{
    const bool editable = contains(getWorld(parent).state.value(), WorldState::Edit);

    // Example: add "create" method
    create = Method<3dSoundList, std::shared_ptr<3dSound>>(
        *this, "create", [this]() {
            auto& world = getWorld(parent());
            auto sound = std::make_shared<3dSound>(world, world.getUniqueId("3dSound"));
            addObject(sound);
            return sound;
        });

    Attributes::addDisplayName(create, "Create 3D Sound");
    Attributes::addEnabled(create, editable);
}

TableModelPtr 3dSoundList::getModel()
{
    return std::make_shared<3dSoundListTableModel>(*this);
}

void 3dSoundList::worldEvent(WorldState state, WorldEvent event)
{
    ObjectList<3dSound>::worldEvent(state, event);
    const bool editable = contains(state, WorldState::Edit);
    Attributes::setEnabled(create, editable);
}

bool 3dSoundList::isListedProperty(std::string_view name)
{
    return 3dSoundListTableModel::isListedProperty(name);
}
