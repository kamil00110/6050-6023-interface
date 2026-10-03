/**
 * client/src/widget/camera/cameraoverviewwidget.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "cameraoverviewwidget.hpp"
#include <algorithm>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QFrame>
#include <QResizeEvent>
#include "camerawidget.hpp"
#include "../../network/connection.hpp"
#include "../../network/object.hpp"
#include "../../network/abstractproperty.hpp"
#include "../../network/property.hpp"
#include "../../network/objectvectorproperty.hpp"
#include "../../network/error.hpp"
#include <traintastic/locale/locale.hpp>

namespace {
  constexpr int kTileW = 240;
  constexpr int kTileH = 200;
  constexpr int kSpacing = 10;
}

CameraOverviewWidget::CameraOverviewWidget(std::shared_ptr<Connection> connection, QWidget* parent)
  : QWidget(parent)
  , m_connection(std::move(connection))
{
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);

  m_scroll = new QScrollArea(this);
  m_scroll->setWidgetResizable(true);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  outer->addWidget(m_scroll);

  m_container = new QWidget(m_scroll);
  m_grid = new QGridLayout(m_container);
  m_grid->setContentsMargins(kSpacing, kSpacing, kSpacing, kSpacing);
  m_grid->setSpacing(kSpacing);
  m_scroll->setWidget(m_container);

  m_emptyLabel = new QLabel(m_container);
  m_emptyLabel->setAlignment(Qt::AlignCenter);
  m_emptyLabel->setStyleSheet(QStringLiteral("color:#888; padding:24px;"));
  m_emptyLabel->hide();
  m_grid->addWidget(m_emptyLabel, 0, 0);

  m_listRequestId = m_connection->getObject(QStringLiteral("world.cameras"),
    [this](const ObjectPtr& obj, std::optional<const Error> /*error*/)
    {
      m_listRequestId = -1;
      onListReceived(obj);
    });
}

CameraOverviewWidget::~CameraOverviewWidget()
{
  if(m_listRequestId != -1)
    m_connection->cancelRequest(m_listRequestId);
  if(m_itemsRequestId != -1)
    m_connection->cancelRequest(m_itemsRequestId);
}

void CameraOverviewWidget::onListReceived(const ObjectPtr& obj)
{
  if(!obj || !(m_items = obj->getObjectVectorProperty(QStringLiteral("items"))))
  {
    m_emptyLabel->setText(Locale::tr("camera:no_cameras"));
    m_emptyLabel->show();
    return;
  }
  m_listObject = obj;
  connect(m_items, &ObjectVectorProperty::valueChanged, this, &CameraOverviewWidget::rebuild);
  rebuild();
}

void CameraOverviewWidget::rebuild()
{
  if(!m_items)
    return;

  if(m_itemsRequestId != -1)
  {
    m_connection->cancelRequest(m_itemsRequestId);
    m_itemsRequestId = -1;
  }
  clearTiles();

  if(m_items->empty())
  {
    m_emptyLabel->setText(Locale::tr("camera:no_cameras"));
    m_emptyLabel->show();
    return;
  }
  m_emptyLabel->hide();

  m_itemsRequestId = m_items->getObjects(
    [this](const std::vector<ObjectPtr>& cameras, std::optional<const Error> /*error*/)
    {
      m_itemsRequestId = -1;
      buildTiles(cameras);
    });
}

void CameraOverviewWidget::buildTiles(const std::vector<ObjectPtr>& cameras)
{
  clearTiles();
  m_cameras = cameras; // retain so the camera objects (and their streams) stay alive

  for(const auto& cam : cameras)
  {
    if(!cam)
      continue;
    AbstractProperty* idProp = cam->getProperty(QStringLiteral("id"));
    const QString id = idProp ? idProp->toString() : QString();
    if(id.isEmpty())
      continue;

    auto* tile = new QFrame(m_container);
    tile->setFrameShape(QFrame::StyledPanel);
    tile->setFixedSize(kTileW, kTileH);
    auto* v = new QVBoxLayout(tile);
    v->setContentsMargins(4, 4, 4, 4);
    v->setSpacing(3);

    auto* nameLabel = new QLabel(tile);
    nameLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
    AbstractProperty* nameProp = cam->getProperty(QStringLiteral("name"));
    const QString initialName = nameProp ? nameProp->toString() : QString();
    nameLabel->setText(initialName.isEmpty() ? id : initialName);
    if(auto* np = dynamic_cast<Property*>(nameProp))
      connect(np, &Property::valueChanged, nameLabel,
        [np, nameLabel, id]()
        {
          const QString n = np->toString();
          nameLabel->setText(n.isEmpty() ? id : n);
        });

    auto* preview = new CameraWidget(m_connection, id, tile);

    v->addWidget(nameLabel);
    v->addWidget(preview, 1);

    m_tiles.push_back(tile);
  }
  relayout();
}

void CameraOverviewWidget::clearTiles()
{
  for(auto* t : m_tiles)
  {
    m_grid->removeWidget(t);
    t->deleteLater();
  }
  m_tiles.clear();
  m_cameras.clear();
  m_columns = 0;
}

int CameraOverviewWidget::computeColumns() const
{
  const int w = m_scroll->viewport()->width();
  return std::max(1, (w - kSpacing) / (kTileW + kSpacing));
}

void CameraOverviewWidget::relayout()
{
  const int cols = computeColumns();
  const int n = static_cast<int>(m_tiles.size());

  for(auto* t : m_tiles)
    m_grid->removeWidget(t);

  // Reset previous stretch spacers, then pack tiles top-left.
  for(int c = 0; c < 64; ++c)
    m_grid->setColumnStretch(c, 0);
  for(int r = 0; r <= n + 1; ++r)
    m_grid->setRowStretch(r, 0);

  for(int i = 0; i < n; ++i)
    m_grid->addWidget(m_tiles[i], i / cols, i % cols, Qt::AlignTop | Qt::AlignLeft);

  const int rows = (n + cols - 1) / cols;
  m_grid->setColumnStretch(cols, 1);
  m_grid->setRowStretch(rows, 1);
  m_columns = cols;
}

void CameraOverviewWidget::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  if(!m_tiles.empty() && computeColumns() != m_columns)
    relayout();
}
