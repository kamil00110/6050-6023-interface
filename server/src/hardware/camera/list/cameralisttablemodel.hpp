/**
 * server/src/hardware/camera/list/cameralisttablemodel.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_CAMERA_LIST_CAMERALISTTABLEMODEL_HPP
#define TRAINTASTIC_SERVER_HARDWARE_CAMERA_LIST_CAMERALISTTABLEMODEL_HPP

#include "../../../core/objectlisttablemodel.hpp"
#include "cameralistcolumn.hpp"
#include "../camera.hpp"

class CameraList;

class CameraListTableModel : public ObjectListTableModel<Camera>
{
  friend class CameraList;

private:
  std::vector<CameraListColumn> m_columns;
  void changed(uint32_t row, CameraListColumn column);

protected:
  void propertyChanged(BaseProperty& property, uint32_t row) final;

public:
  CLASS_ID("camera_list_table_model")

  static bool isListedProperty(std::string_view name);

  CameraListTableModel(CameraList& list);
  std::string getText(uint32_t column, uint32_t row) const final;
};

#endif
