/**
 * client/src/widget/propertyslider.hpp
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

#ifndef TRAINTASTIC_CLIENT_WIDGET_PROPERTYSLIDER_HPP
#define TRAINTASTIC_CLIENT_WIDGET_PROPERTYSLIDER_HPP

#include <QWidget>
#include <QSlider>
#include <QString>
#include <QColor>

class QLabel;
class QEvent;
class QPaintEvent;
class Property;

/**
 * @brief Custom-painted horizontal slider: a pill handle, palette-themed colours
 *        that dim when disabled, and a configurable track fill.
 *
 * The fill marks the "amount" part of the track. Left (the usual) fills from the
 * minimum to the handle; Right fills from the handle to the maximum; Center fills
 * between the track midpoint and the handle (for signed ranges where filling from
 * the minimum would be misleading, e.g. brightness -100..100); Off draws no fill.
 */
class PropertySliderBar : public QSlider
{
  public:
    enum class Fill { Off, Left, Right, Center };

    explicit PropertySliderBar(QWidget* parent = nullptr);

    void setFill(Fill fill);
    void setColors(const QColor& groove, const QColor& accent, const QColor& handle,
                   const QColor& disabled);

  protected:
    void paintEvent(QPaintEvent* event) override;

  private:
    Fill m_fill{Fill::Left};
    QColor m_groove;
    QColor m_accent;
    QColor m_handle;
    QColor m_disabled;
};

/**
 * @brief Horizontal slider bound to an integer Property, with a numeric readout.
 *
 * Honours the usual property attributes: Min/Max (range), Step (single step, and
 * snapping of the committed value when step > 1), Unit (suffix on the readout)
 * and Enabled/Visible. The track fill is chosen from the range -- centered for a
 * signed (bipolar) range, left-filled otherwise.
 */
class PropertySlider : public QWidget
{
  Q_OBJECT

  protected:
    Property& m_property;
    PropertySliderBar* m_slider;
    QLabel* m_valueLabel;
    QString m_unit;
    int m_requestId;

    void cancelRequest();
    void updateRange();
    void updateStep();
    void updateFill();  ///< pick the track fill from the range (centered for bipolar)
    void updateValueLabel(int value);
    void applyColors(); ///< push palette colours into the bar

    void changeEvent(QEvent* event) override; ///< re-theme the bar when the palette changes

  public:
    PropertySlider(Property& property, QWidget* parent = nullptr);
    ~PropertySlider() override;
};

#endif
