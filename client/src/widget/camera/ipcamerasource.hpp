/**
 * client/src/widget/camera/ipcamerasource.hpp
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
#ifndef TRAINTASTIC_CLIENT_WIDGET_CAMERA_IPCAMERASOURCE_HPP
#define TRAINTASTIC_CLIENT_WIDGET_CAMERA_IPCAMERASOURCE_HPP

#include <QThread>
#include <QImage>
#include <QString>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace cv { class VideoCapture; class Mat; }

/**
 * Captures a network camera (RTSP / MJPEG / RTMP / HLS) DIRECTLY on the client
 * with OpenCV, decoding frames locally and emitting them as QImages. The client
 * is on the same network as the camera, so capturing here avoids the server
 * pulling the stream and re-encoding it to MJPEG -- which would send the video
 * over the network twice (camera->server, then server->client).
 *
 * This mirrors the server's former IpCameraCapture: the same backend fallbacks
 * (GStreamer -> FFmpeg -> any), the same reconnect-with-backoff, the same frame
 * rate cap and flip -- but it paints the frames instead of re-encoding them to
 * JPEG ("without translation"). Like the server, the decode calls are contained
 * by C++ try/catch and (on Windows) SEH so a faulty stream reconnects instead of
 * crashing the client.
 *
 * Lifetime: create with no parent; the object lives on the creating (GUI) thread
 * while run() executes on this QThread. It self-deletes when run() returns
 * (finished -> deleteLater, wired in the constructor). run() returns only once
 * stop() has been called, so the owner's pointer is always cleared first -- it
 * must drop the pointer in stop() and never touch the object again. stop() is
 * fire-and-forget and never blocks the GUI thread.
 *
 * fps and flip are live-adjustable (atomic) via setFps()/setFlip() without a
 * reconnect; the source URL and request-from-source/resolution are fixed for the
 * lifetime of a source (changing them means creating a new IpCameraSource).
 */
class IpCameraSource final : public QThread
{
  Q_OBJECT

public:
  IpCameraSource(const QString& url, double fps,
                 uint32_t reqWidth, uint32_t reqHeight, int jpegQuality,
                 bool flipVertical, bool flipHorizontal, bool appendSpecs,
                 QObject* parent = nullptr);
  ~IpCameraSource() override;

  /// Flag the capture loop to stop; the object self-destructs asynchronously
  /// when the loop exits. Never blocks. After this, drop the pointer.
  void stop();

  /// Live-adjustable (thread-safe): applied on the next frame, no reconnect.
  void setFps(double fps) { m_fps.store(fps > 0.0 ? fps : 1.0); }
  void setFlip(bool vertical, bool horizontal)
  {
    m_flipVertical.store(vertical);
    m_flipHorizontal.store(horizontal);
  }

signals:
  void frameReady(const QImage& image); ///< a decoded frame, delivered on the GUI thread
  void streamConnected();               ///< the source opened / reconnected
  void streamLost();                    ///< the source dropped; reconnecting

protected:
  void run() override;

private:
  bool openCapture();                 ///< open m_url via the backend fallbacks (worker thread)
  int  readFrameGuarded(cv::Mat& frame); ///< SEH-guarded grab+retrieve; -1 lost, 0 skip, 1 frame
  int  readFrameRaw(cv::Mat& frame);     ///< the actual grab+retrieve (no SEH here)
  bool framePeriodElapsed();          ///< frame-rate cap (worker thread only)
  void reconnectWait();               ///< bounded, stop-responsive backoff before a reconnect

  std::string                           m_url;
  std::atomic<double>                   m_fps;
  std::atomic<bool>                     m_flipVertical;
  std::atomic<bool>                     m_flipHorizontal;
  std::unique_ptr<cv::VideoCapture>     m_cap;
  int                                   m_backend{0}; ///< cv::VideoCaptureAPIs of the open capture
  std::atomic<bool>                     m_interrupted{false};
  std::chrono::steady_clock::time_point m_lastFrameTime{};

  static constexpr int k_reconnectWaitMs = 2000;
  static constexpr int k_openTimeoutMs   = 10000;
  static constexpr int k_readTimeoutMs   = 10000;
};

#endif
