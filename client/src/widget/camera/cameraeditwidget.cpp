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
#include <QLabel>
#include <QLineEdit>

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
#include "../propertyvaluelabel.hpp"
#include "../createwidget.hpp"
#include <traintastic/locale/locale.hpp>

// CameraType::Local == 0, keep in sync with server shared/src/traintastic/enum/cameratype.hpp
static constexpr int64_t kCameraTypeLocal = 0;

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
    const QString objectId = m_object->getProperty("id")
                               ? m_object->getProperty("id")->toString()
                               : QString();
    auto* preview = new CameraWidget(MainWindow::instance->connection(), objectId, this);
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

  // Generic helper for standard property rows
  const auto addRow = [&](const char* propName)
  {
    AbstractProperty* ap = m_object->getProperty(propName);
    if(!ap)
      return;
    if(Property* p = dynamic_cast<Property*>(ap))
      form->addRow(new InterfaceItemNameLabel(*ap, formContainer),
                   createWidget(*p, formContainer));
  };

  addRow("name");
  addRow("type");

  // ── device: two rows, one enabled at a time based on type ────────────
  //
  // Row 1 — Local camera dropdown:
  //   Bound to the device property. Server fills Values/AliasKeys/AliasValues
  //   with discovered camera names when type is Local.
  //   Disabled when type is RTSP or MJPEG.
  //
  // Row 2 — IP camera URL:
  //   Plain QLineEdit manually synced to device property.
  //   Enabled only when type is RTSP or MJPEG.
  //   Disabled when type is Local.

  Property* deviceProp = dynamic_cast<Property*>(m_object->getProperty("device"));
  Property* typeProp   = dynamic_cast<Property*>(m_object->getProperty("type"));

  PropertyComboBox* deviceCombo = nullptr;
  QLineEdit*        urlEdit     = nullptr;

  if(deviceProp)
  {
    // Row 1: local camera selector
    deviceCombo = new PropertyComboBox(*deviceProp, formContainer);
    form->addRow(new QLabel(Locale::tr("camera:local_camera"), formContainer), deviceCombo);

    // Row 2: IP camera URL — plain QLineEdit to avoid signal conflicts
    // with PropertyComboBox being bound to the same property.
    urlEdit = new QLineEdit(formContainer);
    urlEdit->setPlaceholderText(QStringLiteral("rtsp://user:pass@192.168.1.100/stream"));
    urlEdit->setText(QString::fromStdString(deviceProp->toString()));

    // User finishes editing → push value to server
    connect(urlEdit, &QLineEdit::editingFinished,
      [urlEdit, deviceProp]()
      {
        deviceProp->setValueString(urlEdit->text());
      });

    // Server updates value (e.g. loaded from file) → update text field
    connect(deviceProp, &Property::valueChangedString,
      [urlEdit](const QString& value)
      {
        if(urlEdit->text() != value)
          urlEdit->setText(value);
      });

    form->addRow(new QLabel(Locale::tr("camera:url"), formContainer), urlEdit);
  }

  // Enable/disable the two device rows based on current type and the
  // server-side Enabled attribute (which is false outside edit mode).
  const auto applyTypeState = [deviceCombo, urlEdit](int64_t typeValue, bool serverEnabled)
  {
    const bool isLocal = (typeValue == kCameraTypeLocal);
    if(deviceCombo)
      deviceCombo->setEnabled(serverEnabled && isLocal);
    if(urlEdit)
      urlEdit->setEnabled(serverEnabled && !isLocal);
  };

  if(deviceProp && typeProp)
  {
    // Apply immediately with current values
    const bool serverEnabled = deviceProp->getAttributeBool(AttributeName::Enabled, true);
    applyTypeState(typeProp->toInt64(), serverEnabled);

    // React to type changes
    connect(typeProp, &Property::valueChangedInt64,
      [applyTypeState, deviceProp](int64_t newType)
      {
        const bool en = deviceProp->getAttributeBool(AttributeName::Enabled, true);
        applyTypeState(newType, en);
      });

    // React to edit mode toggling (server sets Enabled on/off)
    connect(deviceProp, &Property::attributeChanged,
      [applyTypeState, typeProp](AttributeName name, const QVariant& value)
      {
        if(name == AttributeName::Enabled)
          applyTypeState(typeProp->toInt64(), value.toBool());
      });
  }

  // ── Remaining properties ──────────────────────────────────────────────
  addRow("fps");
  addRow("enabled");

  // Read-only status properties
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
