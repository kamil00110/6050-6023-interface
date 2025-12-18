#include "3dSoundList.hpp"
#include "../../world/world.hpp"
#include "../../core/attributes.hpp"

Sound3DList::Sound3DList(Object& _parent, std::string_view parentPropertyName)
    : ObjectList<Sound3D>(_parent, parentPropertyName)
    , create(*this, "create", [this]()
    {
        auto& world = dynamic_cast<World&>(parent());
        return Sound3D::create(world, world.getUniqueId(Sound3D::defaultId));
    })
    , delete_(*this, "delete", [this](const ObjectPtr& obj)
    {
        removeObject(obj);
        return true;
    })
{
    m_interfaceItems.add(create);
    m_interfaceItems.add(delete_);
}

TableModelPtr Sound3DList::getModel()
{
    return {}; // placeholder
}

void Sound3DList::worldEvent(WorldState state, WorldEvent event)
{
    ObjectList<Sound3D>::worldEvent(state, event);
}

bool Sound3DList::isListedProperty(std::string_view name)
{
    return false;
}
