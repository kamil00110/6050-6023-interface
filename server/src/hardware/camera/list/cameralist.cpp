/**
 * server/src/hardware/camera/list/cameralist.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */

#include "cameralist.hpp"
#include "cameralisttablemodel.hpp"
#include "../camera.hpp"
#include "../../../world/getworld.hpp"
#include "../../../core/attributes.hpp"
#include "../../../core/method.tpp"
#include "../../../utils/displayname.hpp"

CameraList::CameraList(Object& parent, std::string_view parentPropertyName, CameraListColumn columns_)
  : ObjectList<Camera>(parent, parentPropertyName)
  , columns(columns_)
  , create{*this, "create",
      [this]()
      {
        auto& world = getWorld(parent());
        return Camera::create(world, world.getUniqueId(Camera::defaultId));
      }}
  , delete_{*this, "delete", std::bind(&CameraList::deleteMethodHandler, this, std::placeholders::_1)}
{
  const bool editable = contains(getWorld(parent()).state.value(), WorldState::Edit);

  Attributes::addDisplayName(create,  DisplayName::List::create);
  Attributes::addEnabled(create, editable);
  m_interfaceItems.add(create);

  Attributes::addDisplayName(delete_, DisplayName::List::delete_);
  Attributes::addEnabled(delete_, editable);
  m_interfaceItems.add(delete_);
}

TableModelPtr CameraList::getModel()
{
  return std::make_shared<CameraListTableModel>(*this);
}

void CameraList::worldEvent(WorldState state, WorldEvent event)
{
  ObjectList<Camera>::worldEvent(state, event);
  const bool editable = contains(state, WorldState::Edit);
  Attributes::setEnabled(create,  editable);
  Attributes::setEnabled(delete_, editable);
}

bool CameraList::isListedProperty(std::string_view name)
{
  return CameraListTableModel::isListedProperty(name);
}
