/**
 * client/src/widget/propertyslider.cpp
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

#include "propertyslider.hpp"
#include <limits>
#include <QSlider>
#include <QLabel>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QColor>
#include <QPalette>
#include <QEvent>
#include "../network/property.hpp"
#include "../network/object.hpp"
#include "../network/connection.hpp"
#include "../network/error.hpp"

PropertySlider::PropertySlider(Property& property, QWidget* parent) :
  QWidget(parent),
  m_property{property},
  m_slider{new QSlider(Qt::Horizontal, this)},
  m_valueLabel{new QLabel(this)},
  m_requestId{Connection::invalidRequestId}
{
  Q_ASSERT(m_property.type() == ValueType::Integer);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_slider, 1);
  layout->addWidget(m_valueLabel);
  m_valueLabel->setMinimumWidth(44);
  m_valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  applyThumbStyle();

  setEnabled(m_property.getAttributeBool(AttributeName::Enabled, true));
  setVisible(m_property.getAttributeBool(AttributeName::Visible, true));
  updateRange();
  updateStep();
  m_unit = m_property.getAttributeString(AttributeName::Unit, QString());
  {
    QSignalBlocker block(m_slider);
    m_slider->setValue(m_property.toInt());
  }
  updateValueLabel(m_property.toInt());

  connect(&m_property, &AbstractProperty::valueChangedInt, this,
    [this](int value)
    {
      if(!m_slider->isSliderDown())
      {
        QSignalBlocker block(m_slider);
        m_slider->setValue(value);
      }
      updateValueLabel(value);
    });

  connect(&m_property, &AbstractProperty::attributeChanged, this,
    [this](AttributeName name, const QVariant& value)
    {
      switch(name)
      {
        case AttributeName::Enabled:
          setEnabled(value.toBool());
          break;

        case AttributeName::Visible:
          setVisible(value.toBool());
          break;

        case AttributeName::Min:
        case AttributeName::Max:
          updateRange();
          break;

        case AttributeName::Step:
          updateStep();
          break;

        case AttributeName::Unit:
          m_unit = value.toString();
          updateValueLabel(m_slider->value());
          break;

        default:
          break;
      }
    });

  connect(m_slider, &QSlider::valueChanged, this,
    [this](int value)
    {
      // Honour Step for the committed value: snap to the step grid (relative to
      // the minimum). Only when a meaningful step is set, so the default
      // single-unit slider still allows every value.
      const int step = m_slider->singleStep();
      if(step > 1)
      {
        const int min = m_slider->minimum();
        const int snapped = qBound(min, min + ((value - min + step / 2) / step) * step,
                                   m_slider->maximum());
        if(snapped != value)
        {
          QSignalBlocker block(m_slider);
          m_slider->setValue(snapped);
          value = snapped;
        }
      }
      updateValueLabel(value);
      cancelRequest();
      m_requestId = m_property.setValueInt(value,
        [this](std::optional<const Error> /*error*/) { });
    });
}

void PropertySlider::changeEvent(QEvent* event)
{
  QWidget::changeEvent(event);
  if(event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
    applyThumbStyle(); // keep the pill colours in sync with a runtime theme change
}

PropertySlider::~PropertySlider()
{
  cancelRequest();
}

void PropertySlider::cancelRequest()
{
  if(m_requestId != Connection::invalidRequestId)
  {
    m_property.object().connection()->cancelRequest(m_requestId);
    m_requestId = Connection::invalidRequestId;
  }
}

void PropertySlider::updateRange()
{
  m_slider->setRange(
    m_property.getAttributeInt(AttributeName::Min, std::numeric_limits<int>::min()),
    m_property.getAttributeInt(AttributeName::Max, std::numeric_limits<int>::max()));
}

void PropertySlider::updateStep()
{
  const int step = m_property.getAttributeInt(AttributeName::Step, m_slider->singleStep());
  if(step > 0)
    m_slider->setSingleStep(step);
}

void PropertySlider::updateValueLabel(int value)
{
  m_valueLabel->setText(m_unit.isEmpty()
    ? QString::number(value)
    : QStringLiteral("%1 %2").arg(value).arg(m_unit));
}

void PropertySlider::applyThumbStyle()
{
  // Pill-shaped handle drawn from the palette so it tracks the active theme.
  const QColor groove = palette().color(QPalette::Mid);
  const QColor accent = palette().color(QPalette::Highlight);
  const QColor handle = palette().color(QPalette::Light);
  m_slider->setStyleSheet(QStringLiteral(
      "QSlider::groove:horizontal { height: 6px; border-radius: 3px; background: %1; }"
      "QSlider::add-page:horizontal { height: 6px; border-radius: 3px; background: %1; }"
      "QSlider::sub-page:horizontal { height: 6px; border-radius: 3px; background: %2; }"
      "QSlider::handle:horizontal { width: 12px; height: 20px; margin: -7px 0;"
      " border-radius: 6px; background: %3; border: 1px solid %2; }"
      "QSlider::handle:horizontal:disabled { background: %1; border: 1px solid %1; }")
    .arg(groove.name(), accent.name(), handle.name()));
}
