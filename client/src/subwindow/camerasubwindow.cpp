/**
 * client/src/subwindow/camerasubwindow.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */

#include "camerasubwindow.hpp"
#include "../widget/camera/camerawidget.hpp"
#include "../network/connection.hpp"
#include <traintastic/locale/locale.hpp>

CameraSubWindow* CameraSubWindow::create(std::shared_ptr<Connection> connection,
                                         const QString& cameraObjectId,
                                         QWidget* parent)
{
  return new CameraSubWindow(std::move(connection), cameraObjectId, parent);
}

CameraSubWindow::CameraSubWindow(std::shared_ptr<Connection> connection,
                                 const QString& cameraObjectId,
                                 QWidget* parent)
  : SubWindow(SubWindowType::Camera, parent)
  , m_cameraWidget(new CameraWidget(connection, cameraObjectId, this))
{
  setWidget(m_cameraWidget);
  setWindowTitle(Locale::tr("camera:camera"));

  // Default size — user can resize freely
  resize(480, 360);

  // Title updates when the camera name property changes
  connection->getObject(cameraObjectId,
    [this](const ObjectPtr& obj, std::optional<const Error>)
    {
      if(!obj) return;
      if(auto* name = obj->getProperty("name"))
      {
        // Set initial title
        if(!name->toString().isEmpty())
          setWindowTitle(name->toString());

        // Track future renames
        connect(name, &AbstractProperty::valueChangedString, this,
          [this](const QString& newName)
          {
            if(!newName.isEmpty())
              setWindowTitle(newName);
          });
      }
    });
}
