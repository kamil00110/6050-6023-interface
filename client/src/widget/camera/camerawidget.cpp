/**
 * client/src/widget/camera/camerawidget.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2025 Reinder Feenstra
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
#include "../../network/connection.hpp"
#include "../../network/object.hpp"
#include "../../network/property.hpp"
#include "../../network/error.hpp"
#include <traintastic/locale/locale.hpp>

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
      if(!obj)
      {
        showStatus(Locale::tr("camera:not_found"));
        return;
      }

      if(auto* prop = obj->getProperty("stream_url"))
      {
        // Connect to future changes
        connect(prop, &AbstractProperty::valueChangedString, this,
          [this](const QString& path)
          {
            setStreamPath(path);
          });

        // Use current value immediately
        setStreamPath(prop->toString());
      }
    });
}

CameraWidget::~CameraWidget()
{
  stopStream();
}

// ─── Public API ──────────────────────────────────────────────────────────────

void CameraWidget::setStreamPath(const QString& urlPath)
{
  if(m_streamPath == urlPath)
    return;

  stopStream();
  m_streamPath = urlPath;

  if(m_streamPath.isEmpty())
  {
    showStatus(Locale::tr("camera:disabled"));
    return;
  }

  if(m_active)
    startStream();
}

void CameraWidget::setActive(bool active)
{
  m_active = active;
  if(active && !m_reply && !m_streamPath.isEmpty())
    startStream();
  else if(!active)
    stopStream();
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
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = nullptr;
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

  // Auto-reconnect after a short delay unless we were deliberately stopped
  if(m_active && !m_streamPath.isEmpty())
  {
    QTimer::singleShot(2000, this,
      [this]()
      {
        if(m_active && !m_reply)
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
  // bytes as the JPEG payload, display, then advance past it.

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

      const QByteArray jpegData = m_buffer.mid(soi, eoi - soi + 2);
      m_buffer.remove(0, eoi + 2);

      QPixmap px;
      if(px.loadFromData(jpegData, "JPEG"))
        showPixmap(px);
      continue;
    }

    // We know Content-Length: wait until we have the full payload
    const int payloadStart = sep + 4;
    if(m_buffer.size() < payloadStart + contentLength)
      break; // need more data

    const QByteArray jpegData = m_buffer.mid(payloadStart, contentLength);
    m_buffer.remove(0, payloadStart + contentLength);

    // Strip leading \r\n boundary separator if present
    if(m_buffer.startsWith("\r\n"))
      m_buffer.remove(0, 2);

    QPixmap px;
    if(px.loadFromData(jpegData, "JPEG"))
      showPixmap(px);
  }
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
