/**
 * server/src/hardware/3dSound/3dSound.hpp
 *
 * Defines a single 3D sound object.
 */

#pragma once

#include "../../world/world.hpp"
#include "../../core/idobject.hpp"
#include "../../core/property.hpp"
#include "../../core/method.hpp"
#include "../../core/event.hpp"

class Sound3DList;

class Sound3D : public IdObject
{
public:
  Sound3D(World& world, std::string_view _id);

  void addToWorld();
  void loaded();
  void destroying();
  void worldEvent(WorldState state, WorldEvent event);

  Method onEvent;

  Property<std::string> name;
  Property<std::shared_ptr<Sound3DController>> controller;
  Property<float> x;
  Property<float> y;
  Property<float> z;
  Property<float> volume;
  Property<float> pitch;

private:
  void controllerChanged();
};

