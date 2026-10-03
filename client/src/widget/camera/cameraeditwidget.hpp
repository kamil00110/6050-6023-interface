/**
 * client/src/widget/camera/cameraeditwidget.hpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef TRAINTASTIC_CLIENT_WIDGET_CAMERA_CAMERAEDITWIDGET_HPP
#define TRAINTASTIC_CLIENT_WIDGET_CAMERA_CAMERAEDITWIDGET_HPP

#include "../object/abstracteditwidget.hpp"

class CameraWidget;

/**
 * @brief Settings panel for a Camera object.
 *
 * Replaces the generic ObjectEditWidget for classId "camera".
 *
 * Layout:
 *   ┌─────────────────────────────────┐
 *   │  CameraWidget  (live preview)   │  min 200 px, expands
 *   ├─────────────────────────────────┤
 *   │  name      │ <line-edit>        │
 *   │  type      │ <combo>            │
 *   │  device    │ <combo/editable>   │  ← server pushes Values for Local type
 *   │  fps       │ <spin>             │
 *   │  enabled   │ <check>            │
 *   │  stream_url│ <label>            │
 *   │  width     │ <label>            │
 *   │  height    │ <label>            │
 *   └─────────────────────────────────┘
 *
 * The `device` row is always a PropertyComboBox (String properties are
 * always editable in QComboBox). When the camera type is Local the server
 * populates the Values / AliasKeys / AliasValues attributes with discovered
 * cameras; when it is RTSP or MJPEG those attributes are cleared so the user
 * can type a URL directly into the (now empty) editable combo.
 */
class CameraEditWidget : public AbstractEditWidget
{
  Q_OBJECT

protected:
  void buildForm() final;

public:
  explicit CameraEditWidget(const ObjectPtr& object, QWidget* parent = nullptr);
};

#endif
