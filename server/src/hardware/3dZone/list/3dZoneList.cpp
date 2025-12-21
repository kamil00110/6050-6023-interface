#include "3dZoneList.hpp"
#include "3dZoneListTableModel.hpp"
#include "../3dZone.hpp"
#include "../../../world/getworld.hpp"
#include "../../../core/attributes.hpp"
#include "../../../core/method.tpp"
#include "../../../utils/displayname.hpp"

ThreeDZoneList::ThreeDZoneList(Object& _parent, std::string_view parentPropertyName, ThreeDZoneListColumn _columns)
  : ObjectList<ThreeDZone>(_parent, parentPropertyName)
  , columns{_columns}
  , create{*this, "create",
      [this]()
      {
        auto& world = getWorld(parent());
        auto zone = ThreeDZone::create(world, world.getUniqueId(ThreeDZone::defaultId));
        return zone;
      }}
  , delete_{*this, "delete", std::bind(&ThreeDZoneList::deleteMethodHandler, this, std::placeholders::_1)}
{
  const bool editable = contains(getWorld(parent()).state.value(), WorldState::Edit);
  
  Attributes::addDisplayName(create, DisplayName::List::create);
  Attributes::addEnabled(create, editable);
  m_interfaceItems.add(create);
  
  Attributes::addDisplayName(delete_, DisplayName::List::delete_);
  Attributes::addEnabled(delete_, editable);
  m_interfaceItems.add(delete_);
}

TableModelPtr ThreeDZoneList::getModel()
{
  return std::make_shared<ThreeDZoneListTableModel>(*this);
}

void ThreeDZoneList::worldEvent(WorldState state, WorldEvent event)
{
  ObjectList<ThreeDZone>::worldEvent(state, event);
  
  const bool editable = contains(state, WorldState::Edit);
  Attributes::setEnabled(create, editable);
  Attributes::setEnabled(delete_, editable);
}

bool ThreeDZoneList::isListedProperty(std::string_view name)
{
  return ThreeDZoneListTableModel::isListedProperty(name);
}
