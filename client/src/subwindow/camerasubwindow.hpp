/**
 * client/src/subwindow/camerasubwindow.hpp
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

#ifndef TRAINTASTIC_CLIENT_SUBWINDOW_CAMERASUBWINDOW_HPP
#define TRAINTASTIC_CLIENT_SUBWINDOW_CAMERASUBWINDOW_HPP

#include "subwindow.hpp"
#include <memory>

class Connection;
class CameraWidget;

/**
 * @brief MDI sub-window hosting a single CameraWidget.
 *
 * Can be opened from the Cameras list in the Objects > Hardware menu, or
 * programmatically (e.g., from a loco tab in the future).
 */
class CameraSubWindow : public SubWindow
{
  Q_OBJECT

public:
  static CameraSubWindow* create(std::shared_ptr<Connection> connection,
                                 const QString& cameraObjectId,
                                 QWidget* parent = nullptr);

  explicit CameraSubWindow(std::shared_ptr<Connection> connection,
                           const QString& cameraObjectId,
                           QWidget* parent = nullptr);

  CameraWidget* cameraWidget() const { return m_cameraWidget; }

private:
  CameraWidget* m_cameraWidget;
};

#endif
