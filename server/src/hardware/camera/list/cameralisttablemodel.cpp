/**
 * server/src/hardware/camera/list/cameralisttablemodel.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */

#include "cameralisttablemodel.hpp"
#include "cameralist.hpp"
#include "../../../utils/displayname.hpp"

bool CameraListTableModel::isListedProperty(std::string_view name)
{
  return
    name == "id"      ||
    name == "name"    ||
    name == "type"    ||
    name == "device"  ||
    name == "enabled";
}

static std::string_view displayName(CameraListColumn col)
{
  switch(col)
  {
    case CameraListColumn::Id:      return DisplayName::Object::id;
    case CameraListColumn::Name:    return DisplayName::Object::name;
    case CameraListColumn::Type:    return "camera:type";
    case CameraListColumn::Device:  return "camera:device";
    case CameraListColumn::Enabled: return "camera:enabled";
  }
  assert(false);
  return {};
}

CameraListTableModel::CameraListTableModel(CameraList& list)
  : ObjectListTableModel<Camera>(list)
{
  std::vector<std::string_view> labels;
  for(auto col : cameraListColumnValues)
  {
    if(contains(list.columns, col))
    {
      labels.emplace_back(displayName(col));
      m_columns.emplace_back(col);
    }
  }
  setColumnHeaders(std::move(labels));
}

std::string CameraListTableModel::getText(uint32_t column, uint32_t row) const
{
  if(row >= rowCount())
    return "";

  const Camera& cam = getItem(row);
  assert(column < m_columns.size());

  switch(m_columns[column])
  {
    case CameraListColumn::Id:
      return cam.id;

    case CameraListColumn::Name:
      return cam.name;

    case CameraListColumn::Type:
    {
      const auto t = cam.type.value();
      switch(t)
      {
        case CameraType::Local: return "Local";
        case CameraType::RTSP:  return "RTSP";
        case CameraType::MJPEG: return "MJPEG";
        case CameraType::RTMP:  return "RTMP";
        case CameraType::HLS:   return "HLS";
      }
      return "";
    }
    case CameraListColumn::Device:
      return cam.device;

    case CameraListColumn::Enabled:
      return cam.enabled ? "yes" : "no";
  }
  assert(false);
  return "";
}

void CameraListTableModel::propertyChanged(BaseProperty& property, uint32_t row)
{
  const std::string_view name = property.name();
  if(name == "id")      changed(row, CameraListColumn::Id);
  else if(name == "name")    changed(row, CameraListColumn::Name);
  else if(name == "type")    changed(row, CameraListColumn::Type);
  else if(name == "device")  changed(row, CameraListColumn::Device);
  else if(name == "enabled") changed(row, CameraListColumn::Enabled);
}

void CameraListTableModel::changed(uint32_t row, CameraListColumn col)
{
  for(size_t i = 0; i < m_columns.size(); i++)
    if(m_columns[i] == col)
    {
      TableModel::changed(row, static_cast<uint32_t>(i));
      return;
    }
}
