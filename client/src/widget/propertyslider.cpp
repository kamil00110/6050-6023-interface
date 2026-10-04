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
#include <QLabel>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QPalette>
#include <QPainter>
#include <QPen>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QEvent>
#include "../network/property.hpp"
#include "../network/object.hpp"
#include "../network/connection.hpp"
#include "../network/error.hpp"

PropertySliderBar::PropertySliderBar(QWidget* parent)
  : QSlider(Qt::Horizontal, parent)
{
  setMinimumHeight(22); // leave room for the pill handle
}

void PropertySliderBar::setFill(Fill fill)
{
  if(fill != m_fill)
  {
    m_fill = fill;
    update();
  }
}

void PropertySliderBar::setColors(const QColor& groove, const QColor& accent,
                                  const QColor& handle, const QColor& disabled)
{
  m_groove = groove;
  m_accent = accent;
  m_handle = handle;
  m_disabled = disabled;
  update();
}

void PropertySliderBar::paintEvent(QPaintEvent*)
{
  QStyleOptionSlider opt;
  initStyleOption(&opt);

  // Use the style's groove/handle geometry so the drawn handle lines up exactly
  // with where the slider hit-tests (no custom metrics -> no interaction drift).
  const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
  const QRect handleRect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
  const bool on = isEnabled();

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::NoPen);

  constexpr int trackHeight = 6;
  const int cy = groove.center().y();
  const QRect track(groove.left(), cy - trackHeight / 2, groove.width(), trackHeight);
  const qreal radius = trackHeight / 2.0;

  painter.setBrush(on ? m_groove : m_disabled);
  painter.drawRoundedRect(track, radius, radius);

  if(on && m_fill != Fill::Off)
  {
    const int hx = handleRect.center().x();
    int x0 = track.left();
    int x1 = hx;
    if(m_fill == Fill::Right)
    {
      x0 = hx;
      x1 = track.right();
    }
    else if(m_fill == Fill::Center)
    {
      const int cx = track.center().x();
      x0 = qMin(cx, hx);
      x1 = qMax(cx, hx);
    }
    if(x1 > x0)
    {
      painter.setBrush(m_accent);
      painter.drawRoundedRect(QRect(x0, track.top(), x1 - x0, trackHeight), radius, radius);
    }
  }

  constexpr int handleWidth = 12;
  constexpr int handleHeight = 20;
  const QRect pill(handleRect.center().x() - handleWidth / 2, cy - handleHeight / 2,
                   handleWidth, handleHeight);
  painter.setPen(QPen(on ? m_accent : m_disabled, 1));
  painter.setBrush(on ? m_handle : m_disabled);
  painter.drawRoundedRect(pill, handleWidth / 2.0, handleWidth / 2.0);
}

PropertySlider::PropertySlider(Property& property, QWidget* parent) :
  QWidget(parent),
  m_property{property},
  m_slider{new PropertySliderBar(this)},
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

  applyColors();

  setEnabled(m_property.getAttributeBool(AttributeName::Enabled, true));
  setVisible(m_property.getAttributeBool(AttributeName::Visible, true));
  updateRange();
  updateStep();
  updateFill();
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
          updateFill(); // a range change can flip bipolar <-> unipolar
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

void PropertySlider::updateFill()
{
  // Center the fill for a signed (bipolar) range -- filling from the minimum
  // would read as a magnitude from the most-negative value, which is wrong for
  // e.g. brightness (-100..0..100). Otherwise fill from the left as usual.
  const int min = m_property.getAttributeInt(AttributeName::Min, 0);
  const int max = m_property.getAttributeInt(AttributeName::Max, 0);
  m_slider->setFill((min < 0 && max > 0)
    ? PropertySliderBar::Fill::Center
    : PropertySliderBar::Fill::Left);
}

void PropertySlider::updateValueLabel(int value)
{
  m_valueLabel->setText(m_unit.isEmpty()
    ? QString::number(value)
    : QStringLiteral("%1 %2").arg(value).arg(m_unit));
}

void PropertySlider::applyColors()
{
  const QPalette pal = palette();
  m_slider->setColors(
    pal.color(QPalette::Mid),
    pal.color(QPalette::Highlight),
    pal.color(QPalette::Light),
    pal.color(QPalette::Disabled, QPalette::Mid));
}

void PropertySlider::changeEvent(QEvent* event)
{
  QWidget::changeEvent(event);
  if(event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
    applyColors(); // keep the bar colours in sync with a runtime theme change
}
