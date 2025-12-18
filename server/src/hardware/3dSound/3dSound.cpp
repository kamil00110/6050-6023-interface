/**
 * server/src/hardware/3dSound/3dSound.cpp
 *
 * Implements the constructor and property registration for 3D sounds.
 */

#include "3dSound.hpp"
#include "list/3dSoundList.hpp"
#include "../../core/attributes.hpp"

Sound3D::Sound3D(World& world, std::string_view _id)
  : IdObject(world, _id)
  , name{this, "name", id, PropertyFlags::ReadWrite | PropertyFlags::Store | PropertyFlags::ScriptReadOnly}
  , controller{this, "controller", nullptr, PropertyFlags::ReadWrite | PropertyFlags::Store,
      [this](const std::shared_ptr<Sound3DController>& /*newValue*/) { controllerChanged(); }}
  , x{this, "x", 0.0f, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , y{this, "y", 0.0f, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , z{this, "z", 0.0f, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , volume{this, "volume", 1.0f, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , pitch{this, "pitch", 1.0f, PropertyFlags::ReadWrite | PropertyFlags::Store}
  , onEvent{*this, "on_event", EventFlags::Scriptable}
{
  const bool editable = contains(m_world.state.value(), WorldState::Edit);

  Attributes::addDisplayName(name, "Name");
  Attributes::addEnabled(name, editable);
  m_interfaceItems.add(name);

  Attributes::addDisplayName(controller, "Controller");
  Attributes::addEnabled(controller, editable);
  Attributes::addObjectList(controller, m_world.sound3DControllers);
  m_interfaceItems.add(controller);

  Attributes::addEnabled(x, editable);
  Attributes::addEnabled(y, editable);
  Attributes::addEnabled(z, editable);
  m_interfaceItems.add(x);
  m_interfaceItems.add(y);
  m_interfaceItems.add(z);

  Attributes::addEnabled(volume, editable);
  Attributes::addEnabled(pitch, editable);
  m_interfaceItems.add(volume);
  m_interfaceItems.add(pitch);

  m_interfaceItems.add(onEvent);
}

void Sound3D::addToWorld()
{
  IdObject::addToWorld();
  m_world.sound3DList->addObject(shared_ptr<Sound3D>());
}

void Sound3D::loaded()
{
  IdObject::loaded();
  controllerChanged();
}

void Sound3D::destroying()
{
  m_world.sound3DList->removeObject(shared_ptr<Sound3D>());
  IdObject::destroying();
}

void Sound3D::worldEvent(WorldState state, WorldEvent event)
{
  IdObject::worldEvent(state, event);
  const bool editable = contains(state, WorldState::Edit);
  Attributes::setEnabled(name, editable);
  Attributes::setEnabled(controller, editable);
  Attributes::setEnabled(x, editable);
  Attributes::setEnabled(y, editable);
  Attributes::setEnabled(z, editable);
  Attributes::setEnabled(volume, editable);
  Attributes::setEnabled(pitch, editable);
}

void Sound3D::controllerChanged()
{
  // TODO: implement behavior when controller changes
}

