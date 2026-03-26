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
#include <traintastic/enum/cameratype.hpp>

// CameraType enum values — keep in sync with
// shared/src/traintastic/enum/cameratype.hpp
static constexpr int64_t kCameraTypeLocal = 0;
static constexpr int64_t kCameraTypeRTSP  = 1;
static constexpr int64_t kCameraTypeMJPEG = 2;

// Detect camera type from URL prefix.
// Returns -1 if the prefix is not recognised (leave type unchanged).
static int64_t detectTypeFromUrl(const QString& url)
{
  if(url.startsWith(QStringLiteral("rtsp://"), Qt::CaseInsensitive) ||
     url.startsWith(QStringLiteral("rtsps://"), Qt::CaseInsensitive))
    return kCameraTypeRTSP;

  if(url.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
     url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive))
    return kCameraTypeMJPEG;

  return -1;
}

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

  // ── Live stream preview ───────────────────────────────────────────────
  {
    const QString objectId = m_object->getProperty("id")
                               ? m_object->getProperty("id")->toString()
                               : QString();
    auto* preview = new CameraWidget(MainWindow::instance->connection(), objectId, this);
    preview->setMinimumHeight(200);
    preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(preview);
  }

  // ── Settings form ─────────────────────────────────────────────────────
  auto* formContainer = new QWidget(this);
  auto* form = new QFormLayout(formContainer);
  form->setContentsMargins(6, 6, 6, 6);
  mainLayout->addWidget(formContainer);
  mainLayout->addStretch(1);

  const auto addRow = [&](const char* propName)
  {
    AbstractProperty* ap = m_object->getProperty(propName);
    if(!ap) return;
    if(Property* p = dynamic_cast<Property*>(ap))
      form->addRow(new InterfaceItemNameLabel(*ap, formContainer),
                   createWidget(*p, formContainer));
  };

  addRow("name");
  addRow("type");

  // ── device — two rows, one active at a time ───────────────────────────
  Property* deviceProp = dynamic_cast<Property*>(m_object->getProperty("device"));
  Property* typeProp   = dynamic_cast<Property*>(m_object->getProperty("type"));

  PropertyComboBox* deviceCombo = nullptr;
  QLineEdit*        urlEdit     = nullptr;

  if(deviceProp)
  {
    // Row 1 — local camera selector (combo populated by server)
    deviceCombo = new PropertyComboBox(*deviceProp, formContainer);
    form->addRow(new QLabel(Locale::tr("camera:local_camera"), formContainer),
                 deviceCombo);

    // Row 2 — IP camera URL (plain line edit, bound to same property)
    urlEdit = new QLineEdit(formContainer);
    urlEdit->setPlaceholderText(
      QStringLiteral("rtsp://192.168.1.100/stream  or  http://192.168.1.100/video"));

    // Initialise with current value only when type is not Local
    if(typeProp && typeProp->toInt64() != kCameraTypeLocal)
      urlEdit->setText(deviceProp->toString());

    // ── Auto-detect type from URL ─────────────────────────────────────
    // When the user finishes editing the URL field:
    //   1. Push the value to the server (device property).
    //   2. If the URL prefix reveals the type (rtsp:// or http://) and
    //      the current type differs, update the type property too so the
    //      server does not have to be told explicitly.
    connect(urlEdit, &QLineEdit::editingFinished,
      [urlEdit, deviceProp, typeProp]()
      {
        const QString url = urlEdit->text().trimmed();

        // Always push device value first
        deviceProp->setValueString(url);

        // Auto-detect and update type if needed
        if(typeProp)
        {
          const int64_t detected = detectTypeFromUrl(url);
          if(detected >= 0 && detected != typeProp->toInt64())
            typeProp->setValueInt64(detected);
        }
      });

    // Server sends a new device value (e.g. loaded from file) -> reflect
    connect(deviceProp, &Property::valueChangedString,
      [urlEdit, typeProp](const QString& value)
      {
        // Only update the URL field when we are in IP mode; in Local mode
        // the value is a numeric index and should not appear in the URL field.
        if(typeProp && typeProp->toInt64() != kCameraTypeLocal)
        {
          if(urlEdit->text() != value)
            urlEdit->setText(value);
        }
      });

    form->addRow(new QLabel(Locale::tr("camera:url"), formContainer), urlEdit);
  }

  // ── Enable / disable rows based on type and server edit permission ────
  const auto applyTypeState = [deviceCombo, urlEdit](int64_t typeValue, bool serverEnabled)
  {
    const bool isLocal = (typeValue == kCameraTypeLocal);
    if(deviceCombo)
      deviceCombo->setEnabled(serverEnabled && isLocal);
    if(urlEdit)
    {
      urlEdit->setEnabled(serverEnabled && !isLocal);
      // Clear the URL field when switching to Local so it never shows
      // a stale URL string while the combo is active.
      if(isLocal)
        urlEdit->clear();
    }
  };

  if(deviceProp && typeProp)
  {
    const bool serverEnabled = deviceProp->getAttributeBool(AttributeName::Enabled, true);
    applyTypeState(typeProp->toInt64(), serverEnabled);

    connect(typeProp, &Property::valueChangedInt64,
      [applyTypeState, deviceProp, urlEdit](int64_t newType)
      {
        const bool en = deviceProp->getAttributeBool(AttributeName::Enabled, true);
        applyTypeState(newType, en);

        // When switching to an IP type, populate the URL field with the
        // current device value (which the server just reset to "0" when
        // switching to Local, so this is safe in both directions).
        if(newType != kCameraTypeLocal && urlEdit)
        {
          const QString current = deviceProp->toString();
          if(urlEdit->text() != current)
            urlEdit->setText(current);
        }
      });

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
    if(!ap) return;
    if(Property* p = dynamic_cast<Property*>(ap))
      form->addRow(new InterfaceItemNameLabel(*ap, formContainer),
                   new PropertyValueLabel(*p, formContainer));
  };

  addReadOnlyRow("stream_url");
  addReadOnlyRow("frame_width");
  addReadOnlyRow("frame_height");

  setLayout(mainLayout);
}
