#include "3dzoneeditorwidget.hpp"
#include <QTimer>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>
#include "../../network/property.hpp"
#include "../../network/method.hpp"
#include "../../network/error.hpp"

constexpr double MIN_DISPLAY_SIZE = 400.0;
constexpr double MARGIN = 40.0;
constexpr double SPEAKER_RADIUS = 10.0;

SpeakerConfigDialog::SpeakerConfigDialog(int speakerId, const QString& label, 
                                         const QString& device, int channel, double volume,
                                         const std::vector<AudioDeviceData>& audioDevices,
                                         const ObjectPtr& zone,
                                         const QPointF& speakerPosition,
                                         QWidget* parent)
  : QDialog(parent)
  , m_audioDevices(audioDevices)
  , m_zone(zone)
  , m_speakerPosition(speakerPosition)
{
  setWindowTitle(QString("Configure Speaker %1 - %2").arg(speakerId + 1).arg(label));
  setModal(true);
  setMinimumWidth(400);
  
  QFormLayout* layout = new QFormLayout(this);
  
  // Sound Controller selector
  m_deviceCombo = new QComboBox(this);
  m_deviceCombo->addItem("(None)", QString());
  
  int selectedIndex = 0;
  for(size_t i = 0; i < m_audioDevices.size(); i++)
  {
    const auto& dev = m_audioDevices[i];
    QString displayName = dev.deviceName;
    if(dev.isDefault)
      displayName += " [Default]";
    
    m_deviceCombo->addItem(displayName, dev.deviceId);
    
    if(dev.deviceId == device)
      selectedIndex = static_cast<int>(i + 1);
  }
  
  m_deviceCombo->setCurrentIndex(selectedIndex);
  
  connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &SpeakerConfigDialog::onDeviceChanged);
  
  layout->addRow("Sound Controller:", m_deviceCombo);
  
  // Speaker/Channel selector
  m_channelCombo = new QComboBox(this);
  layout->addRow("Controller Channel:", m_channelCombo);
  
  // Volume override
  m_volumeSpin = new QDoubleSpinBox(this);
  m_volumeSpin->setRange(0.0, 2.0);
  m_volumeSpin->setSingleStep(0.1);
  m_volumeSpin->setValue(volume);
  m_volumeSpin->setSuffix(" (100% = 1.0)");
  m_volumeSpin->setDecimals(2);
  
  layout->addRow("Volume Override:", m_volumeSpin);
  
  // Test button - NOW FUNCTIONAL
  m_testButton = new QPushButton("Test Speaker", this);
  m_testButton->setToolTip("Play a test sound at this speaker's position");
  connect(m_testButton, &QPushButton::clicked, this, &SpeakerConfigDialog::testThisSpeaker);
  
  layout->addRow("", m_testButton);
  
  // Dialog buttons
  QDialogButtonBox* buttonBox = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  
  layout->addRow(buttonBox);
  
  // Initialize channel combo based on selected device
  onDeviceChanged(selectedIndex);
  if(channel < m_channelCombo->count())
    m_channelCombo->setCurrentIndex(channel);
}
void SpeakerConfigDialog::testThisSpeaker()
{
  if(!m_zone)
    return;
  
  Method* method = m_zone->getMethod("test_sound_at_position");
  if(!method)
  {
    QMessageBox::information(this, "Test Speaker", 
      "Test sound feature not available on this server version.");
    return;
  }
  
  // Convert speaker position from cm to meters
  double xMeters = m_speakerPosition.x() / 100.0;
  double yMeters = m_speakerPosition.y() / 100.0;
  
  // Format arguments as comma-separated string
  QString args = QString("%1,%2").arg(xMeters, 0, 'f', 6).arg(yMeters, 0, 'f', 6);
  
  qDebug() << "Calling test_sound_at_position for speaker at:" << args;
  
  // Call the method
  method->call(args);
  
  // Visual feedback
  m_testButton->setEnabled(false);
  m_testButton->setText("Playing...");
  
  // Re-enable after 2 seconds
  QTimer::singleShot(2000, this, [this]() {
    m_testButton->setEnabled(true);
    m_testButton->setText("Test Speaker");
  });
}

void SpeakerConfigDialog::onDeviceChanged(int index)
{
  m_channelCombo->clear();
  
  if(index <= 0 || index > static_cast<int>(m_audioDevices.size()))
  {
    m_channelCombo->setEnabled(false);
    m_testButton->setEnabled(false);
    return;
  }
  
  m_channelCombo->setEnabled(true);
  m_testButton->setEnabled(true);
  
  const auto& device = m_audioDevices[index - 1];
  
  // Add channels based on device capability
  for(const auto& channel : device.channels)
  {
    m_channelCombo->addItem(
      QString("%1 - %2").arg(channel.index + 1).arg(channel.name),
      channel.index
    );
  }
}

QString SpeakerConfigDialog::getDevice() const
{
  return m_deviceCombo->currentData().toString();
}

int SpeakerConfigDialog::getChannel() const
{
  if(m_channelCombo->count() == 0)
    return 0;
  return m_channelCombo->currentData().toInt();
}

double SpeakerConfigDialog::getVolume() const
{
  return m_volumeSpin->value();
}

ThreeDZoneEditorWidget::ThreeDZoneEditorWidget(const ObjectPtr& zone, QWidget* parent)
  : QWidget(parent)
  , m_zone(zone)
  , m_width(100.0)  // Changed from 1000.0 to 100.0 (1m = 100cm)
  , m_height(100.0)
  , m_speakerCount(4)
  , m_selectedSpeaker(-1)
  , m_scale(1.0)
  , m_showTestDot(false)  // ADD THIS
  , m_testDotTimer(new QTimer(this))  // ADD THIS
{
  setMinimumSize(400, 400);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMouseTracking(true);
  
  // ADD THIS - Setup timer for test dot fade out
  m_testDotTimer->setSingleShot(true);
  connect(m_testDotTimer, &QTimer::timeout, this, &ThreeDZoneEditorWidget::clearTestDot);
  
  if(m_zone)
  {
    if(auto* widthProp = m_zone->getProperty("width"))
    {
      m_width = widthProp->toDouble() * 100.0;
      connect(widthProp, &AbstractProperty::valueChanged, this, &ThreeDZoneEditorWidget::updateFromProperties);
    }
    
    if(auto* heightProp = m_zone->getProperty("height"))
    {
      m_height = heightProp->toDouble() * 100.0;
      connect(heightProp, &AbstractProperty::valueChanged, this, &ThreeDZoneEditorWidget::updateFromProperties);
    }
    
    if(auto* setupProp = m_zone->getProperty("speaker_setup"))
    {
      m_speakerCount = static_cast<int>(setupProp->toInt64());
      connect(setupProp, &AbstractProperty::valueChanged, this, &ThreeDZoneEditorWidget::updateFromProperties);
    }
    
    if(auto* speakersProp = m_zone->getProperty("speakers_data"))
    {
      connect(speakersProp, &AbstractProperty::valueChanged, this, &ThreeDZoneEditorWidget::updateSpeakers);
    }
  }
  
  loadAudioDevices();
  calculateLayout();
  updateSpeakers();
}

QSize ThreeDZoneEditorWidget::sizeHint() const
{
  return QSize(600, 600);
}

QSize ThreeDZoneEditorWidget::minimumSizeHint() const
{
  return QSize(400, 400);
}

void ThreeDZoneEditorWidget::loadAudioDevices()
{
  if(!m_zone)
    return;
  
  // Try to refresh the audio devices list first
  if(Method* refreshMethod = m_zone->getMethod("refresh_audio_devices"))
  {
    refreshMethod->call();
  }
  
  // Now read the audio devices from the property
  auto* devicesProp = m_zone->getProperty("audio_devices_json");
  if(!devicesProp)
  {
    // Fallback
    addDummyDevice();
    return;
  }
  
  // Connect to property changes
  connect(devicesProp, &AbstractProperty::valueChanged, this,
    [this, devicesProp]()
    {
      parseAudioDevices(devicesProp->toString());
    });
  
  // Parse current value
  parseAudioDevices(devicesProp->toString());
}

void ThreeDZoneEditorWidget::parseAudioDevices(const QString& jsonStr)
{
  if(jsonStr.isEmpty())
  {
    addDummyDevice();
    return;
  }
  
  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
  if(!doc.isArray())
  {
    addDummyDevice();
    return;
  }
  
  m_audioDevices.clear();
  
  QJsonArray devices = doc.array();
  for(const QJsonValue& val : devices)
  {
    if(!val.isObject())
      continue;
    
    QJsonObject obj = val.toObject();
    
    AudioDeviceData device;
    device.deviceId = obj["id"].toString();
    device.deviceName = obj["name"].toString();
    device.channelCount = obj["channelCount"].toInt();
    device.isDefault = obj["isDefault"].toBool();
    
    QJsonArray channels = obj["channels"].toArray();
    for(const QJsonValue& chVal : channels)
    {
      if(!chVal.isObject())
        continue;
      
      QJsonObject chObj = chVal.toObject();
      AudioDeviceData::ChannelData channel;
      channel.index = chObj["index"].toInt();
      channel.name = chObj["name"].toString();
      device.channels.push_back(channel);
    }
    
    m_audioDevices.push_back(device);
  }
  
  if(m_audioDevices.empty())
  {
    addDummyDevice();
  }
  
  update();
}
QPointF ThreeDZoneEditorWidget::screenToWorld(const QPointF& screenPos) const
{
  double x = (screenPos.x() - m_zoneRect.left()) / m_scale;
  double y = (screenPos.y() - m_zoneRect.top()) / m_scale;
  return QPointF(x, y);
}

void ThreeDZoneEditorWidget::testSoundAtPosition(const QPointF& worldPos)
{
  if(!m_zone)
    return;
  
  Method* method = m_zone->getMethod("test_sound_at_position");
  if(!method)
  {
    qDebug() << "test_sound_at_position method not found";
    return;
  }
  
  // Convert from cm to meters for server
  double xMeters = worldPos.x() / 100.0;
  double yMeters = worldPos.y() / 100.0;
  
  // Format arguments as comma-separated string
  QString args = QString("%1,%2").arg(xMeters, 0, 'f', 6).arg(yMeters, 0, 'f', 6);
  
  qDebug() << "Calling test_sound_at_position with args:" << args;
  
  // Call the method - no callback, just fire and forget
  method->call(args);
  
  // Show visual feedback
  m_testDotPosition = worldToScreen(worldPos.x(), worldPos.y());
  m_showTestDot = true;
  update();
  
  // Hide dot after 1 second
  m_testDotTimer->start(1000);
}

void ThreeDZoneEditorWidget::clearTestDot()
{
  m_showTestDot = false;
  update();
}

void ThreeDZoneEditorWidget::addDummyDevice()
{
  AudioDeviceData dummy;
  dummy.deviceId = "";
  dummy.deviceName = "(No audio devices detected)";
  dummy.channelCount = 8;
  dummy.isDefault = false;
  
  for(int i = 0; i < 8; i++)
  {
    AudioDeviceData::ChannelData ch;
    ch.index = i;
    ch.name = QString("Channel %1").arg(i + 1);
    dummy.channels.push_back(ch);
  }
  
  m_audioDevices.push_back(dummy);
}

void ThreeDZoneEditorWidget::updateFromProperties()
{
  if(m_zone)
  {
    if(auto* widthProp = m_zone->getProperty("width"))
      m_width = widthProp->toDouble() * 100.0;
    
    if(auto* heightProp = m_zone->getProperty("height"))
      m_height = heightProp->toDouble() * 100.0;
    
    if(auto* setupProp = m_zone->getProperty("speaker_setup"))
      m_speakerCount = static_cast<int>(setupProp->toInt64());
  }
  
  calculateLayout();
  update();
}

void ThreeDZoneEditorWidget::updateSpeakers()
{
  m_speakers.clear();
  
  if(!m_zone)
    return;
  
  auto* speakersProp = m_zone->getProperty("speakers_data");
  if(!speakersProp)
    return;
  
  QString jsonStr = speakersProp->toString();
  if(jsonStr.isEmpty())
    return;
  
  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
  if(!doc.isArray())
    return;
  
  QJsonArray speakers = doc.array();
  for(const QJsonValue& val : speakers)
  {
    if(!val.isObject())
      continue;
    
    QJsonObject obj = val.toObject();
    
    SpeakerInfo info;
    info.id = obj["id"].toInt();
    info.position.setX(obj["x"].toDouble() * 100.0);
    info.position.setY(obj["y"].toDouble() * 100.0);
    info.label = obj["label"].toString();
    info.device = obj["device"].toString();
    info.channel = obj["channel"].toInt();
    info.volume = obj["volume"].toDouble();
    
    m_speakers.append(info);
  }
  
  update();
}

void ThreeDZoneEditorWidget::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  calculateLayout();
}

void ThreeDZoneEditorWidget::calculateLayout()
{
  const double widgetWidth = width() - 2 * MARGIN;
  const double widgetHeight = height() - 2 * MARGIN;
  
  // Calculate scale to fit the zone in the widget while maintaining aspect ratio
  const double scaleX = widgetWidth / m_width;
  const double scaleY = widgetHeight / m_height;
  m_scale = std::min(scaleX, scaleY);
  
  // Center the zone rectangle
  const double rectWidth = m_width * m_scale;
  const double rectHeight = m_height * m_scale;
  const double rectX = (width() - rectWidth) / 2.0;
  const double rectY = (height() - rectHeight) / 2.0;
  
  m_zoneRect = QRectF(rectX, rectY, rectWidth, rectHeight);
}

QPointF ThreeDZoneEditorWidget::worldToScreen(double x, double y) const
{
  return QPointF(
    m_zoneRect.left() + x * m_scale,
    m_zoneRect.top() + y * m_scale
  );
}

int ThreeDZoneEditorWidget::getSpeakerAtPosition(const QPointF& pos) const
{
  for(int i = 0; i < m_speakers.size(); i++)
  {
    QPointF screenPos = worldToScreen(m_speakers[i].position.x(), m_speakers[i].position.y());
    double dx = pos.x() - screenPos.x();
    double dy = pos.y() - screenPos.y();
    double dist = std::sqrt(dx * dx + dy * dy);
    
    // Increased tolerance from 5 to 15 pixels for easier clicking
    if(dist <= SPEAKER_RADIUS + 15)
      return i;
  }
  return -1;
}

void ThreeDZoneEditorWidget::mousePressEvent(QMouseEvent* event)
{
  if(event->button() == Qt::LeftButton)
  {
    // IMPORTANT: Check for speaker clicks FIRST, before checking zone bounds
    // This allows clicking on speakers even if they're on the edge of the zone
    int speakerIdx = getSpeakerAtPosition(event->pos());
    if(speakerIdx >= 0)
    {
      openSpeakerConfig(speakerIdx);
      return;  // Exit early, don't test sound
    }
    
    // If didn't click on a speaker, check if click is inside the zone
    if(m_zoneRect.contains(event->pos()))
    {
      // Clicked in empty zone - test sound at this position
      QPointF worldPos = screenToWorld(event->pos());
      
      // Clamp to zone boundaries
      worldPos.setX(qBound(0.0, worldPos.x(), m_width));
      worldPos.setY(qBound(0.0, worldPos.y(), m_height));
      
      testSoundAtPosition(worldPos);
    }
  }
  QWidget::mousePressEvent(event);
}

void ThreeDZoneEditorWidget::openSpeakerConfig(int speakerId)
{
  if(speakerId < 0 || speakerId >= m_speakers.size())
    return;
  
  const SpeakerInfo& speaker = m_speakers[speakerId];
  
  SpeakerConfigDialog dialog(
    speaker.id,
    speaker.label,
    speaker.device,
    speaker.channel,
    speaker.volume,
    m_audioDevices,
    m_zone,  // ADD THIS
    speaker.position,  // ADD THIS - already in cm
    this
  );
  
  if(dialog.exec() == QDialog::Accepted)
  {
    m_speakers[speakerId].device = dialog.getDevice();
    m_speakers[speakerId].channel = dialog.getChannel();
    m_speakers[speakerId].volume = dialog.getVolume();
    
    saveSpeakersToProperty();
    update();
  }
}
  

void ThreeDZoneEditorWidget::saveSpeakersToProperty()
{
  if(!m_zone)
    return;
  
  auto* speakersProp = m_zone->getProperty("speakers_data");
  if(!speakersProp)
    return;
  
  QJsonArray speakers;
  
  for(const auto& speaker : m_speakers)
  {
    QJsonObject obj;
    obj["id"] = speaker.id;
    obj["x"] = speaker.position.x() / 100.0;
    obj["y"] = speaker.position.y() / 100.0;
    obj["label"] = speaker.label;
    obj["device"] = speaker.device;
    obj["channel"] = speaker.channel;
    obj["volume"] = speaker.volume;
    
    speakers.append(obj);
  }
  
  QJsonDocument doc(speakers);
  speakersProp->setValueString(doc.toJson(QJsonDocument::Compact));
}

QString ThreeDZoneEditorWidget::getDeviceDisplayName(const QString& deviceId) const
{
  for(const auto& device : m_audioDevices)
  {
    if(device.deviceId == deviceId)
      return device.deviceName;
  }
  return "Unknown Device";
}

void ThreeDZoneEditorWidget::paintEvent(QPaintEvent* event)
{
  Q_UNUSED(event);
  
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  
  // Draw background
  painter.fillRect(rect(), QColor(240, 240, 240));
  
  // Draw zone rectangle
  painter.setPen(QPen(QColor(100, 100, 100), 2));
  painter.setBrush(QColor(255, 255, 255));
  painter.drawRect(m_zoneRect);
  
  // Draw dimension labels (in cm)
  painter.setPen(QColor(80, 80, 80));
  QFont font = painter.font();
  font.setPointSize(9);
  painter.setFont(font);
  
  // Width label
  QString widthText = QString("%1 cm").arg(m_width, 0, 'f', 0);
  QRectF widthRect(m_zoneRect.left(), m_zoneRect.bottom() + 5, m_zoneRect.width(), 20);
  painter.drawText(widthRect, Qt::AlignCenter, widthText);
  
  // Height label
  painter.save();
  painter.translate(m_zoneRect.left() - 10, m_zoneRect.center().y());
  painter.rotate(-90);
  QString heightText = QString("%1 cm").arg(m_height, 0, 'f', 0);
  painter.drawText(QRectF(-50, -10, 100, 20), Qt::AlignCenter, heightText);
  painter.restore();
  
  // Draw grid lines
  painter.setPen(QPen(QColor(220, 220, 220), 1, Qt::DashLine));
  const int gridLines = 4;
  for(int i = 1; i < gridLines; i++)
  {
    double xPos = m_zoneRect.left() + (m_zoneRect.width() * i / gridLines);
    double yPos = m_zoneRect.top() + (m_zoneRect.height() * i / gridLines);
    painter.drawLine(QPointF(xPos, m_zoneRect.top()), QPointF(xPos, m_zoneRect.bottom()));
    painter.drawLine(QPointF(m_zoneRect.left(), yPos), QPointF(m_zoneRect.right(), yPos));
  }
  
  // Draw speakers
  for(int i = 0; i < m_speakers.size(); i++)
  {
    const SpeakerInfo& speaker = m_speakers[i];
    QPointF screenPos = worldToScreen(speaker.position.x(), speaker.position.y());
    
    // Draw speaker circle
    bool hasConfig = !speaker.device.isEmpty();
    QColor speakerColor = hasConfig ? QColor(0, 150, 0) : QColor(0, 120, 215);
    
    painter.setPen(QPen(speakerColor, 2));
    painter.setBrush(QColor(speakerColor.red(), speakerColor.green(), speakerColor.blue(), 100));
    painter.drawEllipse(screenPos, SPEAKER_RADIUS, SPEAKER_RADIUS);
    
    // Draw speaker number
    painter.setPen(QColor(255, 255, 255));
    QFont boldFont = font;
    boldFont.setBold(true);
    boldFont.setPointSize(8);
    painter.setFont(boldFont);
    painter.drawText(QRectF(screenPos.x() - 10, screenPos.y() - 10, 20, 20), 
                     Qt::AlignCenter, QString::number(speaker.id + 1));
    
    // Draw speaker label
    painter.setPen(QColor(60, 60, 60));
    painter.setFont(font);
    QRectF labelRect(screenPos.x() - 60, screenPos.y() + SPEAKER_RADIUS + 5, 120, 20);
    painter.drawText(labelRect, Qt::AlignCenter, speaker.label);
    
    // Draw device info if configured
    if(hasConfig)
    {
      painter.setPen(QColor(0, 100, 0));
      QFont smallFont = font;
      smallFont.setPointSize(7);
      painter.setFont(smallFont);
      QString configText = QString("Ch%1").arg(speaker.channel + 1);
      QRectF configRect(screenPos.x() - 60, screenPos.y() + SPEAKER_RADIUS + 22, 120, 15);
      painter.drawText(configRect, Qt::AlignCenter, configText);
    }
  }
  if(m_showTestDot)
  {
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 0, 0, 180));  // Semi-transparent red
    painter.drawEllipse(m_testDotPosition, 8, 8);
    
    // Draw pulsing ring
    painter.setPen(QPen(QColor(255, 0, 0, 120), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(m_testDotPosition, 12, 12);
  }
  
  // Draw speaker count info
  painter.setPen(QColor(100, 100, 100));
  QFont infoFont = font;
  infoFont.setPointSize(10);
  infoFont.setBold(true);
  painter.setFont(infoFont);
  QString speakerInfo = QString("%1 Speakers").arg(m_speakerCount);
  painter.drawText(QRectF(10, 10, 200, 30), Qt::AlignLeft | Qt::AlignVCenter, speakerInfo);
  
  // Draw instruction
  painter.setPen(QColor(120, 120, 120));
  QFont instructionFont = font;
  instructionFont.setPointSize(8);
  instructionFont.setItalic(true);
  painter.setFont(instructionFont);
  painter.drawText(QRectF(10, height() - 25, width() - 20, 20), 
                   Qt::AlignLeft | Qt::AlignVCenter, 
                   "Click on speaker to configure • Click in zone to test sound");  // Updated instruction

}
