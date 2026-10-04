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
#include <QString>

class QSlider;
class QLabel;
class QEvent;
class Property;

/**
 * @brief Horizontal slider bound to an integer Property, with a numeric readout.
 *
 * Honours the usual property attributes: Min/Max (range), Step (single step),
 * Unit (suffix on the readout) and Enabled/Visible. The handle is drawn as a
 * rounded pill using the widget palette so it matches the active theme.
 */
class PropertySlider : public QWidget
{
  Q_OBJECT

  protected:
    Property& m_property;
    QSlider* m_slider;
    QLabel* m_valueLabel;
    QString m_unit;
    int m_requestId;

    void cancelRequest();
    void updateRange();
    void updateStep();
    void updateValueLabel(int value);
    void applyThumbStyle(); ///< pill-shaped handle, palette-themed

    void changeEvent(QEvent* event) override; ///< re-theme the pill when the palette changes

  public:
    PropertySlider(Property& property, QWidget* parent = nullptr);
    ~PropertySlider() override;
};

#endif
