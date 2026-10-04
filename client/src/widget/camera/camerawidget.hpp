/**
 * client/src/widget/camera/camerawidget.hpp
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

#ifndef TRAINTASTIC_CLIENT_WIDGET_CAMERA_CAMERAWIDGET_HPP
#define TRAINTASTIC_CLIENT_WIDGET_CAMERA_CAMERAWIDGET_HPP

#include <QWidget>
#include <QString>
#include <QSize>
#include <QPixmap>
#include <QSharedPointer>
#include <memory>

class QLabel;
class QEvent;
class QShowEvent;
class QResizeEvent;
class Connection;
class CameraStream;

/**
 * @brief A view of a camera stream: shows the frames decoded by a (shared)
 *        CameraStream, scaled to this widget's size.
 *
 * The widget owns no capture itself. It acquires the shared CameraStream for its
 * camera (so the wall tile, the settings preview and any stand-alone window of
 * the same camera all share a single decode), renders the frames it emits, and
 * reports its own visibility so the stream only captures while a view is shown.
 */
class CameraWidget : public QWidget
{
  Q_OBJECT

  public:
    explicit CameraWidget(std::shared_ptr<Connection> connection,
                          const QString& cameraObjectId,
                          QWidget* parent = nullptr);
    ~CameraWidget() override;

    /** Current source frame size of the shared stream, {0,0} if not yet known.
     *  Lets a view attaching to an already-running camera seed its resolution
     *  readout without waiting for the next frameSizeChanged. */
    QSize currentFrameSize() const;

  signals:
    /** Emitted when the displayed frame's source resolution changes. Used to show
     *  the real stream resolution for IP cameras, which the server does not
     *  report. Forwarded from the shared CameraStream. */
    void frameSizeChanged(int width, int height);

  protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    // Watches the enclosing MDI sub-window's visibility (installed in showEvent).
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void setStreamActive(bool active); ///< ref/deref the shared stream on visibility
    void showPixmap(const QPixmap& px);
    void showStatus(const QString& text);

    QSharedPointer<CameraStream> m_stream;
    QLabel*                      m_videoLabel;
    QLabel*                      m_statusLabel;
    QPixmap                      m_frame;                   ///< last full-size frame, so resize rescales from the source
    bool                         m_active{false};           ///< currently counted as a visible viewer (and painting)
    bool                         m_subwindowWatched{false}; ///< installed the visibility filter yet
};

#endif
