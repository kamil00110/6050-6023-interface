/**
 * server/src/hardware/camera/list/cameralist.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_LIST_CAMERALIST_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_LIST_CAMERALIST_HPP

#include "../../../core/objectlist.hpp"
#include "cameralistcolumn.hpp"
#include "../../../core/method.hpp"

class Camera;

class CameraList : public ObjectList<Camera>
{
  CLASS_ID("list.camera")

protected:
  void worldEvent(WorldState state, WorldEvent event) final;
  bool isListedProperty(std::string_view name) final;

public:
  const CameraListColumn columns;

  Method<std::shared_ptr<Camera>()>                        create;
  Method<void(const std::shared_ptr<Camera>&)>             delete_;

  CameraList(Object& parent, std::string_view parentPropertyName, CameraListColumn columns);

  TableModelPtr getModel() final;
};

#endif
