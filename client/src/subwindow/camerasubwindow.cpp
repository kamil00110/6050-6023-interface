/**
 * client/src/subwindow/camerasubwindow.cpp
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

#include "camerasubwindow.hpp"
#include "../widget/camera/camerawidget.hpp"
#include "../theme/theme.hpp"
#include "../network/connection.hpp"
#include "../network/object.hpp"
#include "../network/abstractproperty.hpp"
#include "../network/error.hpp"
#include <traintastic/locale/locale.hpp>

CameraSubWindow* CameraSubWindow::create(std::shared_ptr<Connection> connection,
                                         const QString& cameraObjectId,
                                         QWidget* parent)
{
  return new CameraSubWindow(std::move(connection), cameraObjectId, parent);
}

CameraSubWindow::~CameraSubWindow()
{
  // Cancel the in-flight name lookup so its callback can't fire into this
  // destroyed window (use-after-free when closed before the response arrives).
  if(m_objectRequestId != -1 && m_connection)
    m_connection->cancelRequest(m_objectRequestId);
}

CameraSubWindow::CameraSubWindow(std::shared_ptr<Connection> connection,
                                 const QString& cameraObjectId,
                                 QWidget* parent)
  : SubWindow(SubWindowType::Camera, parent)
  , m_connection{connection}
  , m_cameraObjectId{cameraObjectId}
  , m_cameraWidget{new CameraWidget(connection, cameraObjectId, this)}
{
  setWidget(m_cameraWidget);
  setWindowTitle(Locale::tr("camera:camera"));
  Theme::setWindowIcon(*this, QStringLiteral("camera")); // match the other tabs, not the Qt default
  // Initial size comes from defaultSize() / saved geometry via SubWindow::showEvent.

  // Track name property for window title
  m_objectRequestId = connection->getObject(cameraObjectId,
    [this](const ObjectPtr& obj, std::optional<const Error> /*err*/)
    {
      m_objectRequestId = -1; // request completed; don't cancel a reused id later
      if(!obj)
        return;
      if(auto* name = obj->getProperty("name"))
      {
        if(!name->toString().isEmpty())
          setWindowTitle(name->toString());

        connect(name, &AbstractProperty::valueChangedString, this,
          [this](const QString& newName)
          {
            if(!newName.isEmpty())
              setWindowTitle(newName);
          });
      }
    });
}
