/**
 * client/src/widget/camera/camerawidget.cpp
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

#include "camerawidget.hpp"
#include <QVBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QPixmap>
#include <QEvent>
#include <QMdiSubWindow>
#include "capture/camerastream.hpp"
#include <traintastic/locale/locale.hpp>

CameraWidget::CameraWidget(std::shared_ptr<Connection> connection,
                           const QString& cameraObjectId,
                           QWidget* parent)
  : QWidget(parent)
{
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMinimumSize(160, 120);
  setStyleSheet("background: #111;");

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_videoLabel = new QLabel(this);
  m_videoLabel->setAlignment(Qt::AlignCenter);
  m_videoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  layout->addWidget(m_videoLabel);

  m_statusLabel = new QLabel(this);
  m_statusLabel->setAlignment(Qt::AlignCenter);
  m_statusLabel->setStyleSheet("color: #aaa; font-size: 12px; padding: 4px;");
  m_statusLabel->hide();
  layout->addWidget(m_statusLabel);

  // Share a single capture+decode with every other view of this camera.
  m_stream = CameraStream::acquire(std::move(connection), cameraObjectId);
  // Only a visible view paints: a hidden view keeps its stream reference (so the
  // single decode persists for the visible ones) but skips the per-frame scale.
  connect(m_stream.data(), &CameraStream::frameReady, this,
    [this](const QPixmap& frame) { if(m_active) showPixmap(frame); });
  connect(m_stream.data(), &CameraStream::statusChanged, this,
    [this](const QString& text) { if(m_active) showStatus(text); });
  connect(m_stream.data(), &CameraStream::frameSizeChanged, this, &CameraWidget::frameSizeChanged);

  // Render whatever the stream already has, so a view attaching to a running
  // camera is not blank until the next frame.
  if(const QPixmap& frame = m_stream->lastFrame(); !frame.isNull())
    showPixmap(frame);
  else
    showStatus(m_stream->status());
}

QSize CameraWidget::currentFrameSize() const
{
  return m_stream ? m_stream->frameSize() : QSize();
}

CameraWidget::~CameraWidget()
{
  if(m_active && m_stream)
    m_stream->removeVisibleViewer();
  // Dropping m_stream here releases our share; the stream is torn down when the
  // last view is gone.
}

void CameraWidget::setStreamActive(bool active)
{
  if(active == m_active || !m_stream)
    return;
  m_active = active;
  if(active)
  {
    m_stream->addVisibleViewer();
    // We skipped painting while hidden, so bring this view up to date now.
    if(const QPixmap& frame = m_stream->lastFrame(); !frame.isNull())
      showPixmap(frame);
    else
      showStatus(m_stream->status());
  }
  else
    m_stream->removeVisibleViewer();
}

void CameraWidget::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  // Rescale from the stored full-size frame (not the already-downscaled label
  // pixmap), so resizing -- including the first layout after attaching to a
  // running stream -- stays crisp. Smooth here as it is a one-off, not per frame.
  if(!m_frame.isNull())
    m_videoLabel->setPixmap(
      m_frame.scaled(m_videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void CameraWidget::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);
  // On first show (now that we are in the widget hierarchy), watch the enclosing
  // MDI sub-window's visibility: a nested widget does not receive its own hide
  // event when an ancestor window is hidden, so pausing has to key off the
  // sub-window. This covers the wall tiles, the settings preview and the
  // stand-alone camera windows alike.
  if(!m_subwindowWatched)
  {
    for(QWidget* w = parentWidget(); w; w = w->parentWidget())
    {
      if(qobject_cast<QMdiSubWindow*>(w))
      {
        w->installEventFilter(this);
        m_subwindowWatched = true;
        break;
      }
    }
  }
  setStreamActive(true);
}

bool CameraWidget::eventFilter(QObject* watched, QEvent* event)
{
  // watched is the enclosing MDI sub-window (see showEvent): pause the stream
  // while it is hidden so background tabs/windows stop decoding, resume on show.
  if(event->type() == QEvent::Hide)
    setStreamActive(false);
  else if(event->type() == QEvent::Show)
    setStreamActive(true);
  return QWidget::eventFilter(watched, event);
}

void CameraWidget::showPixmap(const QPixmap& px)
{
  m_statusLabel->hide();
  m_videoLabel->show();
  m_frame = px; // keep the full-size frame so resizeEvent rescales from the source
  // Fast (nearest/bilinear) scaling on the per-frame hot path: smooth scaling
  // every frame across several open previews (wall tiles + settings + windows)
  // is a large, needless CPU cost for live video. The one-off resizeEvent scale
  // stays smooth so a paused/last frame still looks clean.
  m_videoLabel->setPixmap(
    px.scaled(m_videoLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
}

void CameraWidget::showStatus(const QString& text)
{
  m_frame = QPixmap(); // drop the frame so a later resize cannot repaint it under the status
  m_videoLabel->clear();
  m_statusLabel->setText(text.isEmpty() ? Locale::tr("camera:connecting") : text);
  m_statusLabel->show();
}
