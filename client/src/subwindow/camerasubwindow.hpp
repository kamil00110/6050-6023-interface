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

#ifndef TRAINTASTIC_CLIENT_SUBWINDOW_CAMERASUBWINDOW_HPP
#define TRAINTASTIC_CLIENT_SUBWINDOW_CAMERASUBWINDOW_HPP

#include "subwindow.hpp"
#include <memory>
#include <QString>

class Connection;
class CameraWidget;

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
  ~CameraSubWindow() override;

  CameraWidget* cameraWidget() const { return m_cameraWidget; }

protected:
  // SubWindow pure virtual — camera sets its widget directly, not via this path
  QWidget* createWidget(const ObjectPtr& /*object*/) override { return nullptr; }
  QSize defaultSize() const override { return QSize(480, 360); }

private:
  std::shared_ptr<Connection> m_connection;
  QString       m_cameraObjectId;
  CameraWidget* m_cameraWidget;
  int           m_objectRequestId{-1};
};

#endif
