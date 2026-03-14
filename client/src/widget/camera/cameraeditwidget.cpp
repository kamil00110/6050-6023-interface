/**
 * client/src/widget/camera/cameraeditwidget.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 */

#include "cameraeditwidget.hpp"

#include <QVBoxLayout>
#include <QFormLayout>

#include "camerawidget.hpp"
#include "../../mainwindow.hpp"
#include "../../network/connection.hpp"
#include "../../network/object.hpp"
#include "../../network/abstractproperty.hpp"
#include "../../network/property.hpp"
#include "../interfaceitemnamelabel.hpp"
#include "../propertycheckbox.hpp"
#include "../propertycombobox.hpp"
#include "../propertydoublespinbox.hpp"
#include "../propertylineedit.hpp"
#include "../propertyvaluelabel.hpp"
#include "../createwidget.hpp"

CameraEditWidget::CameraEditWidget(const ObjectPtr& object, QWidget* parent)
  : AbstractEditWidget(object, parent)
{
  buildForm();
}

void CameraEditWidget::buildForm()
{
  setObjectWindowTitle();

  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // ── Live stream preview ──────────────────────────────────────────────
  {
    auto conn = MainWindow::instance->connection();
    const QString objectId = QString::fromStdString(m_object->getObjectId());
    auto* preview = new CameraWidget(conn, objectId, this);
    preview->setMinimumHeight(200);
    preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(preview);
  }

  // ── Settings form ────────────────────────────────────────────────────
  auto* formContainer = new QWidget(this);
  auto* form = new QFormLayout(formContainer);
  form->setContentsMargins(6, 6, 6, 6);
  mainLayout->addWidget(formContainer);
  mainLayout->addStretch(1);

  // Helper: add a row if the property exists and is a plain (non-object) Property.
  // Uses the standard createWidget() logic for all properties except device.
  const auto addRow = [&](const char* propName)
  {
    AbstractProperty* ap = m_object->getProperty(propName);
    if(!ap)
      return;
    auto* label = new InterfaceItemNameLabel(*ap, formContainer);
    if(Property* p = dynamic_cast<Property*>(ap))
      form->addRow(label, createWidget(*p, formContainer));
  };

  addRow("name");
  addRow("type");

  // ── device ─ always a PropertyComboBox ──────────────────────────────
  // For Local type the server fills Values/AliasKeys/AliasValues with
  // discovered cameras; PropertyComboBox picks these up via attributeChanged.
  // For RTSP / MJPEG the server clears those attributes; because String
  // properties are always editable the user can type a URL directly.
  if(AbstractProperty* ap = m_object->getProperty("device"))
  {
    if(Property* p = dynamic_cast<Property*>(ap))
    {
      auto* label = new InterfaceItemNameLabel(*ap, formContainer);
      auto* combo = new PropertyComboBox(*p, formContainer);
      form->addRow(label, combo);
    }
  }

  addRow("fps");
  addRow("enabled");

  // ── Read-only status properties ──────────────────────────────────────
  const auto addReadOnlyRow = [&](const char* propName)
  {
    AbstractProperty* ap = m_object->getProperty(propName);
    if(!ap)
      return;
    if(Property* p = dynamic_cast<Property*>(ap))
      form->addRow(new InterfaceItemNameLabel(*ap, formContainer),
                   new PropertyValueLabel(*p, formContainer));
  };

  addReadOnlyRow("stream_url");
  addReadOnlyRow("frame_width");
  addReadOnlyRow("frame_height");

  setLayout(mainLayout);
}
