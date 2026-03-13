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
 */

#ifndef TRAINTASTIC_CLIENT_WIDGET_CAMERA_CAMERAWIDGET_HPP
#define TRAINTASTIC_CLIENT_WIDGET_CAMERA_CAMERAWIDGET_HPP

#include <QWidget>
#include <QUrl>
#include <memory>

class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class Connection;

/**
 * @brief Widget that fetches and displays a MJPEG camera stream from the server.
 *
 * The server exposes each camera as an MJPEG stream at
 *   http://<host>:<port>/camera/<id>/stream
 *
 * This widget opens a single persistent GET request to that URL and parses
 * the multipart/x-mixed-replace response in a streaming fashion:
 *   - Accumulates bytes into m_buffer
 *   - Scans for the JPEG SOI marker (0xFF 0xD8) and EOI marker (0xFF 0xD9)
 *   - Decodes and displays each complete JPEG frame via QPixmap
 *
 * No local camera capture takes place on the client; all processing is
 * server-side.  This keeps the Qt client dependency-free of OpenCV.
 */
class CameraWidget : public QWidget
{
  Q_OBJECT

public:
  explicit CameraWidget(std::shared_ptr<Connection> connection,
                        const QString& cameraObjectId,
                        QWidget* parent = nullptr);
  ~CameraWidget() override;

  /** Called when the server sends us a new stream_url property value. */
  void setStreamPath(const QString& urlPath);

  /** Pauses/resumes streaming without closing the connection. */
  void setActive(bool active);

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  std::shared_ptr<Connection> m_connection;
  QString                     m_cameraObjectId;
  QString                     m_streamPath;   ///< e.g. "/camera/camera_01/stream"

  QLabel*                     m_videoLabel;
  QLabel*                     m_statusLabel;

  QNetworkAccessManager*      m_nam;
  QNetworkReply*              m_reply{nullptr};

  QByteArray                  m_buffer;       ///< accumulates raw bytes from reply
  bool                        m_active{true};

  void startStream();
  void stopStream();
  void onReadyRead();
  void onReplyFinished();

  void tryDecodeFrames();
  void showPixmap(const QPixmap& px);
  void showStatus(const QString& text);

  QUrl buildStreamUrl() const;
};

#endif
