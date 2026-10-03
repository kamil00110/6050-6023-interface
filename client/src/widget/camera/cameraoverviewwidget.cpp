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
#include <QAbstractItemModel>
#include "camerawidget.hpp"
#include "../../network/connection.hpp"
#include "../../network/object.hpp"
#include "../../network/abstractproperty.hpp"
#include "../../network/property.hpp"
#include "../../network/tablemodel.hpp"
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
  if(m_modelRequestId != -1)
    m_connection->cancelRequest(m_modelRequestId);
  for(int rid : m_nameRequests)
    m_connection->cancelRequest(rid);
}

void CameraOverviewWidget::onListReceived(const ObjectPtr& obj)
{
  if(!obj)
  {
    m_emptyLabel->setText(Locale::tr("camera:no_cameras"));
    m_emptyLabel->show();
    return;
  }
  m_listObject = obj;
  m_modelRequestId = m_connection->getTableModel(obj,
    [this](const TableModelPtr& model, std::optional<const Error> /*error*/)
    {
      m_modelRequestId = -1;
      onTableModel(model);
    });
}

void CameraOverviewWidget::onTableModel(const TableModelPtr& model)
{
  if(!model)
  {
    m_emptyLabel->setText(Locale::tr("camera:no_cameras"));
    m_emptyLabel->show();
    return;
  }
  m_tableModel = model;
  model->setRegionAll(true); // request all cells so getRowObjectId() is populated

  const auto onChange = [this]() { refresh(); };
  connect(model.get(), &QAbstractItemModel::modelReset,   this, onChange);
  connect(model.get(), &QAbstractItemModel::dataChanged,  this, onChange);
  connect(model.get(), &QAbstractItemModel::rowsInserted, this, onChange);
  connect(model.get(), &QAbstractItemModel::rowsRemoved,  this, onChange);
  connect(model.get(), &QAbstractItemModel::layoutChanged, this, onChange);
  refresh();
}

void CameraOverviewWidget::refresh()
{
  if(!m_tableModel)
    return;

  const int rows = m_tableModel->rowCount();
  QStringList ids;
  for(int r = 0; r < rows; ++r)
  {
    const QString id = m_tableModel->getRowObjectId(r);
    if(!id.isEmpty())
      ids.append(id);
  }
  // Rows exist but their ids have not been fetched yet -- wait for the next
  // data signal rather than flashing "no cameras".
  if(rows > 0 && ids.isEmpty())
    return;
  if(ids == m_currentIds)
    return; // the set of cameras did not change -- don't rebuild (keeps streams alive)

  m_currentIds = ids;
  rebuildTiles(ids);
}

void CameraOverviewWidget::rebuildTiles(const QStringList& ids)
{
  clearTiles();

  if(ids.isEmpty())
  {
    m_emptyLabel->setText(Locale::tr("camera:no_cameras"));
    m_emptyLabel->show();
    return;
  }
  m_emptyLabel->hide();

  for(const QString& id : ids)
  {
    auto* tile = new QFrame(m_container);
    tile->setFrameShape(QFrame::StyledPanel);
    tile->setFixedSize(kTileW, kTileH);
    auto* v = new QVBoxLayout(tile);
    v->setContentsMargins(4, 4, 4, 4);
    v->setSpacing(3);

    auto* nameLabel = new QLabel(id, tile);
    nameLabel->setStyleSheet(QStringLiteral("font-weight:600;"));

    auto* preview = new CameraWidget(m_connection, id, tile);

    v->addWidget(nameLabel);
    v->addWidget(preview, 1);
    m_tiles.push_back(tile);

    // Upgrade the tile title from the object id to the camera's name, and keep
    // the object alive (so the cached camera object the stream relies on stays).
    const int rid = m_connection->getObject(id,
      [this, nameLabel, id](const ObjectPtr& obj, std::optional<const Error> /*error*/)
      {
        if(!obj)
          return;
        m_cameras.push_back(obj);
        AbstractProperty* nameProp = obj->getProperty(QStringLiteral("name"));
        if(nameProp && !nameProp->toString().isEmpty())
          nameLabel->setText(nameProp->toString());
        if(auto* np = dynamic_cast<Property*>(nameProp))
          connect(np, &Property::valueChanged, nameLabel,
            [np, nameLabel, id]()
            {
              const QString n = np->toString();
              nameLabel->setText(n.isEmpty() ? id : n);
            });
      });
    m_nameRequests.push_back(rid);
  }
  relayout();
}

void CameraOverviewWidget::clearTiles()
{
  for(int rid : m_nameRequests)
    m_connection->cancelRequest(rid);
  m_nameRequests.clear();

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
