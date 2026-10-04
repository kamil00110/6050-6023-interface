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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include "ipcamerasource.hpp"
#include "../../network/connection.hpp"
#include "../../network/object.hpp"
#include "../../network/property.hpp"
#include "../../network/error.hpp"
#include <traintastic/locale/locale.hpp>
#include <traintastic/enum/cameratype.hpp>
#include <traintastic/enum/cameraresolution.hpp>

// ─── Constructor ─────────────────────────────────────────────────────────────

CameraWidget::CameraWidget(std::shared_ptr<Connection> connection,
                           const QString& cameraObjectId,
                           QWidget* parent)
  : QWidget(parent)
  , m_connection(std::move(connection))
  , m_cameraObjectId(cameraObjectId)
  , m_nam(new QNetworkAccessManager(this))
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

  showStatus(Locale::tr("camera:connecting"));

  // Fetch the camera object so we can read and watch its stream_url property.
  m_objectRequestId = m_connection->getObject(m_cameraObjectId,
    [this](const ObjectPtr& obj, std::optional<const Error> /*err*/)
    {
      m_objectRequestId = -1;
      if(!obj)
      {
        showStatus(Locale::tr("camera:not_found"));
        return;
      }

      // Watch the enabled flag and the stream path so the status shown reflects
      // the real state (disabled vs. connecting vs. streaming).
      if(auto* enabledProp = obj->getProperty("enabled"))
      {
        m_enabled = enabledProp->toBool();
        connect(enabledProp, &AbstractProperty::valueChangedBool, this,
          [this](bool enabled) { m_enabled = enabled; updateState(); });
      }
      if(auto* urlProp = obj->getProperty("stream_url"))
      {
        m_streamPath = urlProp->toString();
        connect(urlProp, &AbstractProperty::valueChangedString, this,
          [this](const QString& path) { m_streamPath = path; updateState(); });
      }

      // type/device pick the path; changing either must fully restart.
      if(auto* p = obj->getProperty("type"))
      {
        m_type = p->toInt64();
        connect(p, &AbstractProperty::valueChangedInt64, this,
          [this](int64_t v) { m_type = v; restart(); });
      }
      if(auto* p = obj->getProperty("device"))
      {
        m_device = p->toString();
        connect(p, &AbstractProperty::valueChangedString, this,
          [this](const QString& v) { m_device = v; restart(); });
      }

      // Specs (applied client-side for IP cameras, server-side for Local):
      // fps and flip are live-adjustable on a running IP source -- push them in
      // place (no reconnect / black-flash, and no stacking of capture threads).
      if(auto* p = obj->getProperty("fps"))
      {
        m_fps = p->toDouble();
        connect(p, &AbstractProperty::valueChangedDouble, this,
          [this](double v) { m_fps = v; if(m_ipSource) m_ipSource->setFps(v); });
      }
      if(auto* p = obj->getProperty("flip_vertical"))
      {
        m_flipVertical = p->toBool();
        connect(p, &AbstractProperty::valueChangedBool, this,
          [this](bool v) { m_flipVertical = v;
            if(m_ipSource) m_ipSource->setFlip(m_flipVertical, m_flipHorizontal); });
      }
      if(auto* p = obj->getProperty("flip_horizontal"))
      {
        m_flipHorizontal = p->toBool();
        connect(p, &AbstractProperty::valueChangedBool, this,
          [this](bool v) { m_flipHorizontal = v;
            if(m_ipSource) m_ipSource->setFlip(m_flipVertical, m_flipHorizontal); });
      }
      // These only affect the source URL (request-from-source query params), so a
      // change requires rebuilding the source -> reconfigureIp() restarts it.
      if(auto* p = obj->getProperty("request_from_source"))
      {
        m_requestFromSource = p->toBool();
        connect(p, &AbstractProperty::valueChangedBool, this,
          [this](bool v) { m_requestFromSource = v; reconfigureIp(); });
      }
      if(auto* p = obj->getProperty("resolution"))
      {
        m_resolution = p->toInt64();
        connect(p, &AbstractProperty::valueChangedInt64, this,
          [this](int64_t v) { m_resolution = v; reconfigureIp(); });
      }
      if(auto* p = obj->getProperty("jpeg_quality"))
      {
        m_jpegQuality = p->toInt();
        connect(p, &AbstractProperty::valueChangedInt, this,
          [this](int v) { m_jpegQuality = v; reconfigureIp(); });
      }

      updateState();
    });
}

CameraWidget::~CameraWidget()
{
  // Cancel the in-flight object request so its callback can never fire into
  // this (now destroyed) widget -- otherwise destroying a camera tile / preview
  // while the getObject response is still in transit is a use-after-free.
  if(m_objectRequestId != -1)
    m_connection->cancelRequest(m_objectRequestId);
  stopStream();
  stopIpStream();
}

// ─── Public API ──────────────────────────────────────────────────────────────

void CameraWidget::setStreamPath(const QString& urlPath)
{
  if(m_streamPath == urlPath)
    return;
  m_streamPath = urlPath;
  updateState();
}

void CameraWidget::setActive(bool active)
{
  m_active = active;
  updateState();
}

void CameraWidget::updateState()
{
  if(!m_enabled)
  {
    stopStream();
    stopIpStream();
    showStatus(Locale::tr("camera:disabled"));
    return;
  }
  if(!m_active)
  {
    stopStream();
    stopIpStream();
    return;
  }

  if(m_type == static_cast<int64_t>(CameraType::Local))
  {
    // Local camera: the server captures it and serves MJPEG; pull from the server.
    stopIpStream();
    if(m_streamPath.isEmpty())
    {
      stopStream();
      showStatus(Locale::tr("camera:connecting")); // enabled but capture not up yet
      return;
    }
    if(!m_reply)
      startStream(); // "connecting" until the first frame arrives
  }
  else
  {
    // Network camera (RTSP/MJPEG/RTMP/HLS): capture the source directly here.
    stopStream();
    if(m_device.isEmpty())
    {
      stopIpStream();
      showStatus(Locale::tr("camera:connecting"));
      return;
    }
    if(!m_ipSource)
      startIpStream(); // "connecting" until the first frame arrives
  }
}

void CameraWidget::restart()
{
  // Full teardown of both paths, then re-pick based on the current type/state.
  stopStream();
  stopIpStream();
  updateState();
}

void CameraWidget::reconfigureIp()
{
  // A spec changed. For IP cameras the specs are applied client-side, so restart
  // the source to pick them up. For Local cameras the server applies them and
  // pushes frames, so nothing to do here.
  if(m_type != static_cast<int64_t>(CameraType::Local) && m_ipSource)
  {
    stopIpStream();
    updateState();
  }
}

// ─── Protected ───────────────────────────────────────────────────────────────

void CameraWidget::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const QPixmap px = m_videoLabel->pixmap();
  if(!px.isNull())
  {
    m_videoLabel->setPixmap(
      px.scaled(m_videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
#else
  const QPixmap* px = m_videoLabel->pixmap();
  if(px && !px->isNull())
  {
    m_videoLabel->setPixmap(
      px->scaled(m_videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
#endif
}
// ─── Private ─────────────────────────────────────────────────────────────────

void CameraWidget::startStream()
{
  Q_ASSERT(!m_reply);
  m_buffer.clear();

  const QUrl url = buildStreamUrl();
  if(!url.isValid())
  {
    showStatus(Locale::tr("camera:invalid_url"));
    return;
  }

  QNetworkRequest req(url);
  req.setRawHeader("Accept", "multipart/x-mixed-replace");
  // Disable automatic redirect following — MJPEG stream should never redirect
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                   QNetworkRequest::ManualRedirectPolicy);

  m_reply = m_nam->get(req);
  connect(m_reply, &QNetworkReply::readyRead,  this, &CameraWidget::onReadyRead);
  connect(m_reply, &QNetworkReply::finished,   this, &CameraWidget::onReplyFinished);
  connect(m_reply, &QNetworkReply::errorOccurred, this,
    [this](QNetworkReply::NetworkError)
    {
      showStatus(Locale::tr("camera:connection_error").arg(m_reply->errorString()));
    });

  showStatus(Locale::tr("camera:connecting"));
}

void CameraWidget::stopStream()
{
  if(m_reply)
  {
    // Detach and null the member BEFORE abort(): abort() can synchronously emit
    // finished()/errorOccurred(), which would re-enter onReplyFinished() and free
    // m_reply underneath us -- then the old code dereferenced a dangling/null
    // m_reply. Disconnect first so no slot fires during teardown.
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
  }
  m_buffer.clear();
}

void CameraWidget::onReadyRead()
{
  m_buffer.append(m_reply->readAll());
  tryDecodeFrames();
}

void CameraWidget::onReplyFinished()
{
  if(m_reply)
  {
    m_reply->deleteLater();
    m_reply = nullptr;
  }

  // Auto-reconnect after a short delay unless we were deliberately stopped. This
  // is the Local (MJPEG-from-server) path only; the fire-time guard re-checks the
  // full Local precondition so a pending timer cannot start a server pull after
  // the camera was switched to an IP type (which captures via m_ipSource and
  // leaves m_reply null / m_streamPath empty).
  if(m_active && m_enabled && m_type == static_cast<int64_t>(CameraType::Local) &&
     !m_streamPath.isEmpty())
  {
    QTimer::singleShot(2000, this,
      [this]()
      {
        if(m_active && m_enabled && !m_reply && !m_ipSource &&
           m_type == static_cast<int64_t>(CameraType::Local) && !m_streamPath.isEmpty())
          startStream();
      });
    showStatus(Locale::tr("camera:reconnecting"));
  }
}

void CameraWidget::tryDecodeFrames()
{
  // JPEG frames are identified by their SOI (0xFF 0xD8) and EOI (0xFF 0xD9) markers.
  // The MJPEG multipart framing (boundary, Content-Length header) is used to find
  // the start of each JPEG payload, but we also accept direct SOI/EOI scanning
  // which is more robust against header variations.
  //
  // Strategy: find Content-Length in the part header, extract exactly that many
  // bytes as the JPEG payload, advance past it. Decoding JPEG is the expensive
  // part, so if several complete frames have piled up in the buffer we keep only
  // the MOST RECENT one and decode just that -- dropping stale frames keeps the
  // preview current and cheap instead of spending CPU decoding frames nobody sees.

  QByteArray latest;

  while(true)
  {
    // Locate the blank line that separates part headers from JPEG data.
    // Part headers look like:
    //   --frame\r\nContent-Type: image/jpeg\r\nContent-Length: N\r\n\r\n
    const int sep = m_buffer.indexOf("\r\n\r\n");
    if(sep < 0)
      break; // need more data

    const QByteArray partHeader = m_buffer.left(sep);

    // Parse Content-Length from the part header
    int contentLength = -1;
    for(const QByteArray& line : partHeader.split('\n'))
    {
      const QByteArray trimmed = line.trimmed();
      if(trimmed.toLower().startsWith("content-length:"))
      {
        bool ok = false;
        contentLength = trimmed.mid(15).trimmed().toInt(&ok);
        if(!ok) contentLength = -1;
        break;
      }
    }

    if(contentLength <= 0)
    {
      // No Content-Length: fall back to SOI/EOI scan
      const int soi = m_buffer.indexOf("\xff\xd8", sep + 4);
      const int eoi = (soi >= 0) ? m_buffer.indexOf("\xff\xd9", soi + 2) : -1;
      if(soi < 0 || eoi < 0)
        break;

      latest = m_buffer.mid(soi, eoi - soi + 2);
      m_buffer.remove(0, eoi + 2);
      continue;
    }

    // We know Content-Length: wait until we have the full payload
    const int payloadStart = sep + 4;
    if(m_buffer.size() < payloadStart + contentLength)
      break; // need more data

    latest = m_buffer.mid(payloadStart, contentLength);
    m_buffer.remove(0, payloadStart + contentLength);

    // Strip leading \r\n boundary separator if present
    if(m_buffer.startsWith("\r\n"))
      m_buffer.remove(0, 2);
  }

  if(!latest.isEmpty())
  {
    QPixmap px;
    if(px.loadFromData(latest, "JPEG"))
      showPixmap(px);
  }
}

// ─── IP path (direct OpenCV capture on the client) ───────────────────────────

void CameraWidget::startIpStream()
{
  Q_ASSERT(!m_ipSource);
  if(m_device.isEmpty())
    return;

  // Request-from-source specs (MJPEG only, mirrors the server's appendSpecs).
  const auto size = toResolutionSize(static_cast<CameraResolution>(m_resolution));
  const bool appendSpecs =
    (m_type == static_cast<int64_t>(CameraType::MJPEG)) && m_requestFromSource;

  // Created with no parent: it self-deletes when its capture loop exits (see
  // IpCameraSource). We only keep a raw pointer and clear it in stopIpStream().
  m_ipSource = new IpCameraSource(m_device, m_fps, size.first, size.second,
                                  m_jpegQuality, m_flipVertical, m_flipHorizontal,
                                  appendSpecs);
  // Guard deliveries by the source pointer: a frame/status already queued from a
  // previous (now stopped/replaced) source has src != m_ipSource and is dropped,
  // so it can't paint a stale frame over the current one (queued cross-thread
  // events aren't retracted by disconnect()).
  IpCameraSource* const src = m_ipSource;
  connect(m_ipSource, &IpCameraSource::frameReady, this,
    [this, src](const QImage& image) { if(src == m_ipSource) showImage(image); });
  connect(m_ipSource, &IpCameraSource::streamLost, this,
    [this, src]() { if(src == m_ipSource) showStatus(Locale::tr("camera:reconnecting")); });
  m_ipSource->start();

  showStatus(Locale::tr("camera:connecting"));
}

void CameraWidget::stopIpStream()
{
  if(m_ipSource)
  {
    m_ipSource->disconnect(this); // stop frames/status reaching this (now) stale view
    m_ipSource->stop();           // fire-and-forget; the source self-deletes when done
    m_ipSource = nullptr;
  }
}

void CameraWidget::showImage(const QImage& image)
{
  if(!image.isNull())
    showPixmap(QPixmap::fromImage(image));
}

void CameraWidget::showPixmap(const QPixmap& px)
{
  m_statusLabel->hide();
  m_videoLabel->show();
  m_videoLabel->setPixmap(
    px.scaled(m_videoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void CameraWidget::showStatus(const QString& text)
{
  m_videoLabel->clear();
  m_statusLabel->setText(text);
  m_statusLabel->show();
}

QUrl CameraWidget::buildStreamUrl() const
{
  if(!m_connection || m_streamPath.isEmpty())
    return {};

  QUrl url;
  url.setScheme(QStringLiteral("http"));
  url.setHost(m_connection->peerAddress().toString());
  url.setPort(m_connection->peerPort());
  url.setPath(m_streamPath);
  return url;
}
