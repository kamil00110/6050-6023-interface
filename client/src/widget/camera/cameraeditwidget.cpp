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
#include <traintastic/locale/locale.hpp>

// CameraType::Local == 0, keep in sync with server enum
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

  // ── device: dropdown for Local, text field for IP ────────────────────
  // Both rows are always present. The one that doesn't apply to the
  // current type is disabled so the user clearly sees both options.
  Property* deviceProp = dynamic_cast<Property*>(m_object->getProperty("device"));
  Property* typeProp   = dynamic_cast<Property*>(m_object->getProperty("type"));

  PropertyComboBox* deviceCombo = nullptr;
  PropertyLineEdit* deviceEdit  = nullptr;

  if(deviceProp)
  {
    // Row 1: dropdown — server fills Values/AliasKeys/AliasValues when Local
    deviceCombo = new PropertyComboBox(*deviceProp, formContainer);
    form->addRow(new QLabel(Locale::tr("camera:local_camera"), formContainer), deviceCombo);

    // Row 2: free-text URL — used for RTSP / MJPEG
    deviceEdit = new PropertyLineEdit(*deviceProp, formContainer);
    deviceEdit->setPlaceholderText(QStringLiteral("rtsp://user:pass@192.168.1.100/stream"));
    form->addRow(new QLabel(Locale::tr("camera:url"), formContainer), deviceEdit);
  }

  // ── Enable/disable rows based on type ────────────────────────────────
  // baseEnabled tracks the server-side Enabled attribute so we respect it.
  const auto applyTypeState = [deviceCombo, deviceEdit](int64_t typeValue, bool serverEnabled)
  {
    const bool isLocal = (typeValue == kCameraTypeLocal);
    if(deviceCombo)
      deviceCombo->setEnabled(serverEnabled && isLocal);
    if(deviceEdit)
      deviceEdit->setEnabled(serverEnabled && !isLocal);
  };

  if(deviceProp && typeProp)
  {
    // Initial state
    const bool serverEnabled = deviceProp->getAttributeBool(AttributeName::Enabled, true);
    applyTypeState(typeProp->toInt64(), serverEnabled);

    // React to type changes
    connect(typeProp, &Property::valueChangedInt64,
      [applyTypeState, deviceProp](int64_t newType)
      {
        const bool en = deviceProp->getAttributeBool(AttributeName::Enabled, true);
        applyTypeState(newType, en);
      });

    // React to the server toggling the Enabled attribute (edit mode on/off)
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
