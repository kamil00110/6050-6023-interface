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
  resize(480, 360);

  // Track name property for window title
  m_objectRequestId = connection->getObject(cameraObjectId,
    [this](const ObjectPtr& obj, std::optional<const Error> /*err*/)
    {
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
