/**
 * client/src/widget/camera/cameraeditwidget.cpp
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

#include "cameraeditwidget.hpp"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSize>
#include <memory>

#include "camerawidget.hpp"
#include "../../mainwindow.hpp"
#include "../../theme/theme.hpp"
#include "../../network/connection.hpp"
#include "../../network/object.hpp"
#include "../../network/abstractproperty.hpp"
#include "../../network/property.hpp"
#include "../interfaceitemnamelabel.hpp"
#include "../propertycheckbox.hpp"
#include "../propertycombobox.hpp"
#include "../propertydoublespinbox.hpp"
#include "../propertyslider.hpp"
#include "../propertyvaluelabel.hpp"
#include "../createwidget.hpp"
#include <traintastic/locale/locale.hpp>

static constexpr int64_t kCameraTypeLocal = 0;
static constexpr int64_t kCameraTypeRTSP  = 1;
static constexpr int64_t kCameraTypeMJPEG = 2;
static constexpr int64_t kCameraTypeRTMP  = 3;
static constexpr int64_t kCameraTypeHLS   = 4;

static int64_t detectTypeFromUrl(const QString& url)
{
  // RTSP / RTSPS
  if(url.startsWith(QStringLiteral("rtsp://"),  Qt::CaseInsensitive) ||
     url.startsWith(QStringLiteral("rtsps://"), Qt::CaseInsensitive))
    return kCameraTypeRTSP;

  // RTMP / RTMPS / RTMPE / RTMPT
  if(url.startsWith(QStringLiteral("rtmp://"),  Qt::CaseInsensitive) ||
     url.startsWith(QStringLiteral("rtmps://"), Qt::CaseInsensitive) ||
     url.startsWith(QStringLiteral("rtmpe://"), Qt::CaseInsensitive) ||
     url.startsWith(QStringLiteral("rtmpt://"), Qt::CaseInsensitive))
    return kCameraTypeRTMP;

  // HLS — m3u8 playlist over HTTP/HTTPS
  if((url.startsWith(QStringLiteral("http://"),  Qt::CaseInsensitive) ||
      url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) &&
     url.endsWith(QStringLiteral(".m3u8"), Qt::CaseInsensitive))
    return kCameraTypeHLS;

  // MJPEG — plain HTTP/HTTPS without m3u8
  if(url.startsWith(QStringLiteral("http://"),  Qt::CaseInsensitive) ||
     url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive))
    return kCameraTypeMJPEG;

  return -1; // unrecognised — leave type unchanged
}

// Grayed-out example URL shown in the (empty) URL field for the selected type.
static QString urlPlaceholderForType(int64_t type)
{
  switch(type)
  {
    case kCameraTypeRTSP:  return QStringLiteral("rtsp://192.168.1.100:554/stream");
    case kCameraTypeRTMP:  return QStringLiteral("rtmp://192.168.1.100/live/stream");
    case kCameraTypeHLS:   return QStringLiteral("http://192.168.1.100/stream.m3u8");
    case kCameraTypeMJPEG:
    default:               return QStringLiteral("http://192.168.1.100/video");
  }
}

// A real stream URL always has a scheme; a bare local-camera index does not.
static bool looksLikeUrl(const QString& value)
{
  return value.contains(QStringLiteral("://"));
}

CameraEditWidget::CameraEditWidget(const ObjectPtr& object, QWidget* parent)
  : AbstractEditWidget(object, parent)
{
  buildForm();
}

void CameraEditWidget::buildForm()
{
  setObjectWindowTitle();
  Theme::setWindowIcon(*this, m_object->classId());

  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // ── Live stream preview ───────────────────────────────────────────────
  CameraWidget* preview = nullptr;
  {
    const QString objectId = m_object->getProperty("id")
                               ? m_object->getProperty("id")->toString()
                               : QString();
    preview = new CameraWidget(MainWindow::instance->connection(), objectId, this);
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
      urlPlaceholderForType(typeProp ? typeProp->toInt64() : kCameraTypeMJPEG));

    // Initialise with current value only when it is a real URL (never the
    // numeric local-camera index).
    if(typeProp && typeProp->toInt64() != kCameraTypeLocal &&
       looksLikeUrl(deviceProp->toString()))
      urlEdit->setText(deviceProp->toString());

    // ── Auto-detect type from URL ─────────────────────────────────────
    // When the user finishes editing the URL field:
    //   1. Push the value to the server (device property).
    //   2. If the URL prefix reveals the type (rtsp:// or http://) and
    //      the current type differs, update the type property too so the
    //      server does not have to be told explicitly.
    connect(urlEdit, &QLineEdit::editingFinished, this,
      [urlEdit, deviceProp, typeProp]()
      {
        const QString url = urlEdit->text().trimmed();

        // editingFinished also fires on plain focus-out (e.g. when the user
        // clicks the type selector). Only act when the value actually changed,
        // so we never push a stale URL or flip the type while the user is
        // switching to another camera type.
        if(url == deviceProp->toString())
          return;

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
    connect(deviceProp, &Property::valueChangedString, this,
      [urlEdit, typeProp](const QString& value)
      {
        // Only update the URL field when we are in IP mode and the value is a
        // real URL; in Local mode the value is a numeric index and must never
        // appear in the URL field.
        if(typeProp && typeProp->toInt64() != kCameraTypeLocal && looksLikeUrl(value))
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
      // a stale URL string while the combo is active; otherwise show a
      // grayed-out example URL for the selected type.
      if(isLocal)
        urlEdit->clear();
      else
        urlEdit->setPlaceholderText(urlPlaceholderForType(typeValue));
    }
  };

  if(deviceProp && typeProp)
  {
    const bool serverEnabled = deviceProp->getAttributeBool(AttributeName::Enabled, true);
    applyTypeState(typeProp->toInt64(), serverEnabled);

    connect(typeProp, &Property::valueChangedInt64, this,
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
          if(looksLikeUrl(current))
          {
            if(urlEdit->text() != current)
              urlEdit->setText(current);
          }
          else
            urlEdit->clear();
        }
      });

    connect(deviceProp, &Property::attributeChanged, this,
      [applyTypeState, typeProp](AttributeName name, const QVariant& value)
      {
        if(name == AttributeName::Enabled)
          applyTypeState(typeProp->toInt64(), value.toBool());
      });
  }

  // ── Remaining properties ──────────────────────────────────────────────
  addRow("request_from_source");
  addRow("fps");
  addRow("resolution");
  addRow("jpeg_quality");
  addRow("flip_vertical");
  addRow("flip_horizontal");

  // brightness slider -- the server hides it for non-local camera types, so the
  // whole row (label + slider) collapses automatically.
  const auto addSliderRow = [&](const char* propName)
  {
    if(Property* p = dynamic_cast<Property*>(m_object->getProperty(propName)))
      form->addRow(new InterfaceItemNameLabel(*p, formContainer),
                   new PropertySlider(*p, formContainer));
  };
  addRow("auto_brightness");
  addSliderRow("brightness");

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

  // Actual stream resolution. Local cameras report it from the server
  // (frame_width/height); IP cameras are decoded on the client, so the server
  // reports 0×0 -- use the size the preview actually decoded (frameSizeChanged),
  // which also works for local cameras. Prefer the client size, fall back to the
  // server properties, else "-".
  {
    Property* wProp = dynamic_cast<Property*>(m_object->getProperty("frame_width"));
    Property* hProp = dynamic_cast<Property*>(m_object->getProperty("frame_height"));
    if(wProp && hProp)
    {
      auto* resLabel = new QLabel(formContainer);
      auto clientSize = std::make_shared<QSize>(0, 0);
      const auto updateRes = [resLabel, wProp, hProp, clientSize]()
      {
        int w = clientSize->width();
        int h = clientSize->height();
        if(w <= 0 || h <= 0) { w = wProp->toInt(); h = hProp->toInt(); }
        resLabel->setText((w > 0 && h > 0)
          ? QStringLiteral("%1 × %2").arg(w).arg(h)
          : QStringLiteral("-"));
      };
      if(preview)
        connect(preview, &CameraWidget::frameSizeChanged, this,
          [clientSize, updateRes](int w, int h) { *clientSize = QSize(w, h); updateRes(); });
      connect(wProp, &Property::valueChanged, this, [updateRes]() { updateRes(); });
      connect(hProp, &Property::valueChanged, this, [updateRes]() { updateRes(); });
      updateRes();
      form->addRow(new QLabel(Locale::tr("camera:stream_resolution"), formContainer),
                   resLabel);
    }
  }

  setLayout(mainLayout);
}
