/**
 * client/src/widget/camera/capture/camerastream.hpp
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

#ifndef TRAINTASTIC_CLIENT_WIDGET_CAMERA_CAPTURE_CAMERASTREAM_HPP
#define TRAINTASTIC_CLIENT_WIDGET_CAMERA_CAPTURE_CAMERASTREAM_HPP

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QPixmap>
#include <QSize>
#include <QUrl>
#include <QHash>
#include <QSharedPointer>
#include <QWeakPointer>
#include <cstdint>
#include <memory>
#include "../../../network/objectptr.hpp"

class QNetworkAccessManager;
class QNetworkReply;
class QImage;
class Connection;
class IpCameraSource;

/**
 * @brief Shared capture + decode for a single camera, reused by every
 *        CameraWidget that displays it (wall tile, settings preview, stand-alone
 *        window).
 *
 * A camera is captured and decoded exactly once no matter how many views show
 * it. The stream owns the capture -- the server MJPEG pull for Local cameras, or
 * a single client-side IpCameraSource for network cameras (RTSP/MJPEG/RTMP/HLS)
 * -- decodes each frame once, and emits the full-size frame to all views, which
 * each scale it to their own size. This avoids N independent decoders for N
 * windows of the same camera.
 *
 * Instances are reference-counted and shared through acquire(): the first view
 * creates the stream, further views of the same camera get the same instance,
 * and the stream is torn down when the last view drops its QSharedPointer.
 * Capture only runs while at least one view is visible, tracked with
 * addVisibleViewer()/removeVisibleViewer().
 *
 * Lives on the GUI thread, like its views; it is not thread-safe.
 */
class CameraStream : public QObject
{
  Q_OBJECT

  public:
    /// Get (or create) the shared stream for a camera. Hold the returned pointer
    /// for as long as the camera is displayed.
    static QSharedPointer<CameraStream> acquire(std::shared_ptr<Connection> connection,
                                                const QString& cameraObjectId);
    ~CameraStream() override;

    void addVisibleViewer();    ///< a view became visible; capture runs while any are
    void removeVisibleViewer(); ///< a view was hidden or destroyed

    const QPixmap& lastFrame() const { return m_lastFrame; } ///< last decoded frame (null if none)
    const QString& status() const { return m_status; }       ///< last status text
    QSize frameSize() const { return QSize(m_lastFrameW, m_lastFrameH); } ///< source size, {0,0} if unknown

  signals:
    void frameReady(const QPixmap& frame);        ///< a decoded, full-size frame
    void statusChanged(const QString& text);      ///< connecting / reconnecting / disabled / ...
    void frameSizeChanged(int width, int height); ///< source resolution changed

  private:
    CameraStream(std::shared_ptr<Connection> connection, const QString& cameraObjectId);

    static QHash<QString, QWeakPointer<CameraStream>>& registry();
    static QString keyFor(const Connection* connection, const QString& cameraObjectId);

    bool active() const { return m_visibleViewers > 0; }
    void updateState();
    void restart();       ///< tear down both paths, then updateState()
    void reconfigureIp(); ///< restart the IP source (if an IP type is streaming)

    // Local path: MJPEG pulled from the server.
    void startStream();
    void stopStream();
    void onReadyRead();
    void onReplyFinished();
    void tryDecodeFrames();
    QUrl buildStreamUrl() const;

    // Network path: a single direct OpenCV capture on the client.
    void startIpStream();
    void stopIpStream();

    void deliverFrame(const QPixmap& frame); ///< cache + emit frameReady (+ frameSizeChanged)
    void setStatus(const QString& text);     ///< cache (and clear the frame) + emit statusChanged

    QString                     m_key;        ///< registry key, so the dtor can deregister
    std::shared_ptr<Connection> m_connection;
    QString                     m_cameraObjectId;
    ObjectPtr                   m_cameraObject; ///< kept alive so the watched properties stay valid
    int                         m_objectRequestId{-1};
    int                         m_visibleViewers{0};

    QString                     m_status;       ///< last status (for a newly-attached view)
    QPixmap                     m_lastFrame;     ///< last decoded frame (for a newly-attached view)
    int                         m_lastFrameW{0};
    int                         m_lastFrameH{0};

    // Local (server MJPEG) path state.
    QString                     m_streamPath;   ///< e.g. "/camera/camera_01/stream"
    QNetworkAccessManager*      m_nam;
    QNetworkReply*              m_reply{nullptr};
    QByteArray                  m_buffer;       ///< accumulates raw bytes from reply
    bool                        m_enabled{true};

    // Camera type + specs, read/watched from the object (same for every view).
    int64_t                     m_type{0};      ///< CameraType; 0 = Local
    QString                     m_device;       ///< source URL (IP cameras)
    double                      m_fps{1.0};
    bool                        m_flipVertical{false};
    bool                        m_flipHorizontal{false};
    bool                        m_requestFromSource{false};
    int64_t                     m_resolution{0}; ///< CameraResolution; 0 = Auto
    int                         m_jpegQuality{75};
    IpCameraSource*             m_ipSource{nullptr};
};

#endif
