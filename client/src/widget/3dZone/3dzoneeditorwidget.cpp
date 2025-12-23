#include "3dzoneeditorwidget.hpp"
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
#include <cmath>
#include "../../network/property.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr double MIN_DISPLAY_SIZE = 400.0;
constexpr double MARGIN = 40.0;
constexpr double SPEAKER_RADIUS = 10.0;

// Speaker Configuration Dialog Implementation
SpeakerConfigDialog::SpeakerConfigDialog(int speakerId, const QString& label, 
                                         const QString& device, int channel, double volume,
                                         QWidget* parent)
  : QDialog(parent)
{
  setWindowTitle(QString("Configure Speaker %1 - %2").arg(speakerId + 1).arg(label));
  setModal(true);
  
  QFormLayout* layout = new QFormLayout(this);
  
  // Sound Controller selector
  m_deviceCombo = new QComboBox(this);
  m_deviceCombo->setEditable(true);
  m_deviceCombo->addItem("(None)");
  m_deviceCombo->addItem("Sound Controller 1");
  m_deviceCombo->addItem("Sound Controller 2");
  m_deviceCombo->addItem("Sound Controller 3");
  m_deviceCombo->addItem("Sound Controller 4");
  
  if(!device.isEmpty())
    m_deviceCombo->setCurrentText(device);
  
  layout->addRow("Sound Controller:", m_deviceCombo);
  
  // Speaker/Channel selector
  m_channelCombo = new QComboBox(this);
  for(int i = 0; i < 16; i++)
  {
    m_channelCombo->addItem(QString("Channel %1").arg(i + 1), i);
  }
  m_channelCombo->setCurrentIndex(channel);
  
  layout->addRow("Controller Channel:", m_channelCombo);
  
  // Volume override
  m_volumeSpin = new QDoubleSpinBox(this);
  m_volumeSpin->setRange(0.0, 2.0);
  m_volumeSpin->setSingleStep(0.1);
  m_volumeSpin->setValue(volume);
  m_volumeSpin->setSuffix(" (100% = 1.0)");
  m_volumeSpin->setDecimals(2);
  
  layout->addRow("Volume Override:", m_volumeSpin);
  
  // Test button
  m_testButton = new QPushButton("Test Speaker", this);
  m_testButton->setToolTip("Test this speaker (not yet implemented)");
  connect(m_testButton, &QPushButton::clicked, this,
    []()
    {
      // TODO: Implement speaker test
      // This will eventually send a test tone to the selected device/channel
    });
  
  layout->addRow("", m_testButton);
  
  // Dialog buttons
  QDialogButtonBox* buttonBox = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  
  layout->addRow(buttonBox);
}

QString SpeakerConfigDialog::getDevice() const
{
  QString device = m_deviceCombo->currentText();
  return (device == "(None)") ? QString() : device;
}

int SpeakerConfigDialog::getChannel() const
{
  return m_channelCombo->currentData().toInt();
}

double SpeakerConfigDialog::getVolume() const
{
  return m_volumeSpin->value();
}

// ThreeDZoneEditorWidget Implementation
ThreeDZoneEditorWidget::ThreeDZoneEditorWidget(const ObjectPtr& zone, QWidget* parent)
  : QWidget(parent)
  , m_zone(zone)
  , m_width(1000.0)  // 10m = 1000cm
  , m_height(1000.0)
  , m_speakerCount(4)
  , m_selectedSpeaker(-1)
  , m_scale(1.0)
{
  setMinimumSize(400, 400);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setMouseTracking(true);
  
  if(m_zone)
  {
    if(auto* widthProp = m_zone->getProperty("width"))
    {
      m_width = widthProp->toDouble() * 100.0; // Convert meters to cm
      connect(widthProp, &AbstractProperty::valueChanged, this, &ThreeDZoneEditorWidget::updateFromProperties);
    }
    
    if(auto* heightProp = m_zone->getProperty("height"))
    {
      m_height = heightProp->toDouble() * 100.0; // Convert meters to cm
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

void ThreeDZoneEditorWidget::updateFromProperties()
{
  if(m_zone)
  {
    if(auto* widthProp = m_zone->getProperty("width"))
      m_width = widthProp->toDouble() * 100.0; // meters to cm
    
    if(auto* heightProp = m_zone->getProperty("height"))
      m_height = heightProp->toDouble() * 100.0; // meters to cm
    
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
  
  try
  {
    json speakers = json::parse(jsonStr.toStdString());
    
    if(speakers.is_array())
    {
      for(const auto& speaker : speakers)
      {
        SpeakerInfo info;
        info.id = speaker.value("id", 0);
        info.position.setX(speaker.value("x", 0.0) * 100.0); // meters to cm
        info.position.setY(speaker.value("y", 0.0) * 100.0);
        info.label = QString::fromStdString(speaker.value("label", ""));
        info.device = QString::fromStdString(speaker.value("device", ""));
        info.channel = speaker.value("channel", 0);
        info.volume = speaker.value("volume", 1.0);
        
        m_speakers.append(info);
      }
    }
  }
  catch(...)
  {
    // JSON parse error - ignore
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
    
    if(dist <= SPEAKER_RADIUS + 5) // Add 5px tolerance
      return i;
  }
  return -1;
}

void ThreeDZoneEditorWidget::mousePressEvent(QMouseEvent* event)
{
  if(event->button() == Qt::LeftButton)
  {
    int speakerIdx = getSpeakerAtPosition(event->pos());
    if(speakerIdx >= 0)
    {
      openSpeakerConfig(speakerIdx);
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
    this
  );
  
  if(dialog.exec() == QDialog::Accepted)
  {
    // Update speaker configuration
    m_speakers[speakerId].device = dialog.getDevice();
    m_speakers[speakerId].channel = dialog.getChannel();
    m_speakers[speakerId].volume = dialog.getVolume();
    
    // Save back to property
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
  
  json speakers = json::array();
  
  for(const auto& speaker : m_speakers)
  {
    speakers.push_back({
      {"id", speaker.id},
      {"x", speaker.position.x() / 100.0}, // cm to meters
      {"y", speaker.position.y() / 100.0},
      {"label", speaker.label.toStdString()},
      {"device", speaker.device.toStdString()},
      {"channel", speaker.channel},
      {"volume", speaker.volume}
    });
  }
  
  speakersProp->setValueString(QString::fromStdString(speakers.dump()));
}

void ThreeDZoneEditorWidget::paintEvent(QPaintEvent* event)
{
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
                   "Click on a speaker to configure");
}
