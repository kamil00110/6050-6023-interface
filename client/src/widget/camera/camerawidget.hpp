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
#include <QUrl>
#include <QImage>
#include <cstdint>
#include <memory>

class QLabel;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class Connection;
class IpCameraSource;

/**
 * @brief Widget that displays a camera stream, by one of two paths chosen from
 *        the camera's type:
 *
 * - Local (hardware) cameras: the server captures them and serves an MJPEG
 *   stream at http://<host>:<port>/camera/<id>/stream. This widget opens a
 *   persistent GET and parses the multipart/x-mixed-replace response (scanning
 *   JPEG SOI/EOI markers), decoding each frame via QPixmap.
 *
 * - Network cameras (RTSP/MJPEG/RTMP/HLS): captured DIRECTLY here via OpenCV
 *   (IpCameraSource) from the camera's `device` URL, with the wished specs (fps
 *   cap, flip) applied locally -- so the server does not pull and re-encode the
 *   stream, which would double the bandwidth. No server stream is used for these.
 *
 * The camera object's `type`/`device` and spec properties are read (and watched)
 * in the getObject callback; updateState() picks the path.
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
  bool                        m_enabled{true};  ///< camera's enabled property
  int                         m_objectRequestId{-1};

  // Camera type + specs, read/watched from the object. For IP types these drive
  // the client-side OpenCV capture (IpCameraSource); for Local they are applied
  // server-side and only `type`/`stream_url` matter here.
  int64_t                     m_type{0};        ///< CameraType; 0 = Local
  QString                     m_device;         ///< source URL (IP cameras)
  double                      m_fps{1.0};
  bool                        m_flipVertical{false};
  bool                        m_flipHorizontal{false};
  bool                        m_requestFromSource{false};
  int64_t                     m_resolution{0};  ///< CameraResolution; 0 = Auto
  int                         m_jpegQuality{75};
  IpCameraSource*             m_ipSource{nullptr}; ///< direct capture for IP cameras

  void updateState();
  void restart();          ///< tear down both paths, then updateState()
  void reconfigureIp();    ///< restart the IP source (if an IP type is streaming)

  // Local path: MJPEG pulled from the server.
  void startStream();
  void stopStream();
  void onReadyRead();
  void onReplyFinished();
  void tryDecodeFrames();
  QUrl buildStreamUrl() const;

  // IP path: direct OpenCV capture on the client.
  void startIpStream();
  void stopIpStream();
  void showImage(const QImage& image);

  void showPixmap(const QPixmap& px);
  void showStatus(const QString& text);
};

#endif
