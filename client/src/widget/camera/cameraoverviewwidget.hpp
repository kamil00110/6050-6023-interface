/**
 * client/src/widget/camera/cameraoverviewwidget.hpp
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

#ifndef TRAINTASTIC_CLIENT_WIDGET_CAMERA_CAMERAOVERVIEWWIDGET_HPP
#define TRAINTASTIC_CLIENT_WIDGET_CAMERA_CAMERAOVERVIEWWIDGET_HPP

#include <QWidget>
#include <QStringList>
#include <memory>
#include <vector>
#include "../../network/objectptr.hpp"
#include "../../network/tablemodelptr.hpp"

class QGridLayout;
class QScrollArea;
class QLabel;
class Connection;

/**
 * @brief A "camera wall": shows every camera in world.cameras as a live tile.
 *
 * The camera list is an ObjectList, enumerated through its TableModel (column 0
 * is the object id). Each tile reuses CameraWidget, so it shows the MJPEG stream
 * and the same status text (connecting / disabled / invalid) as the settings
 * preview. Tiles reflow into columns based on the available width and are only
 * rebuilt when the set of cameras actually changes.
 */
class CameraOverviewWidget : public QWidget
{
  Q_OBJECT

  public:
    explicit CameraOverviewWidget(std::shared_ptr<Connection> connection, QWidget* parent = nullptr);
    ~CameraOverviewWidget() override;

  protected:
    void resizeEvent(QResizeEvent* event) override;

  private:
    std::shared_ptr<Connection> m_connection;
    ObjectPtr m_listObject;
    TableModelPtr m_tableModel;
    std::vector<ObjectPtr> m_cameras;      ///< kept alive so tile streams stay valid
    QStringList m_currentIds;              ///< the camera ids currently shown
    int m_listRequestId = -1;
    int m_modelRequestId = -1;
    std::vector<int> m_nameRequests;       ///< in-flight per-tile name lookups

    QScrollArea* m_scroll;
    QWidget* m_container;
    QGridLayout* m_grid;
    QLabel* m_emptyLabel;
    std::vector<QWidget*> m_tiles;
    int m_columns = 0;

    void onListReceived(const ObjectPtr& obj);
    void onTableModel(const TableModelPtr& model);
    void refresh();
    void rebuildTiles(const QStringList& ids);
    void clearTiles();
    void relayout();
    int  computeColumns() const;
};

#endif
