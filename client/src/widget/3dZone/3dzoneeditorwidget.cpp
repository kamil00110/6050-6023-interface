#include "3dzoneeditorwidget.hpp"
#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>
#include <QLabel>
#include <cmath>
#include "../../network/property.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

constexpr double MIN_DISPLAY_SIZE = 400.0;
constexpr double MARGIN = 40.0;
constexpr double SPEAKER_RADIUS = 8.0;

ThreeDZoneEditorWidget::ThreeDZoneEditorWidget(const ObjectPtr& zone, QWidget* parent)
  : QWidget(parent)
  , m_zone(zone)
  , m_width(10.0)
  , m_height(10.0)
  , m_speakerCount(4)
  , m_scale(1.0)
{
  setMinimumSize(400, 400);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  
  if(m_zone)
  {
    if(auto* widthProp = m_zone->getProperty("width"))
    {
      m_width = widthProp->toDouble();
      connect(widthProp, &AbstractProperty::valueChanged, this, &ThreeDZoneEditorWidget::updateFromProperties);
    }
    
    if(auto* heightProp = m_zone->getProperty("height"))
    {
      m_height = heightProp->toDouble();
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
      m_width = widthProp->toDouble();
    
    if(auto* heightProp = m_zone->getProperty("height"))
      m_height = heightProp->toDouble();
    
    if(auto* setupProp = m_zone->getProperty("speaker_setup"))
      m_speakerCount = static_cast<int>(setupProp->toInt64());
  }
  
  calculateLayout();
  update();
}

void ThreeDZoneEditorWidget::updateSpeakers()
{
  m_speakerPositions.clear();
  m_speakerLabels.clear();
  
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
        double x = speaker.value("x", 0.0);
        double y = speaker.value("y", 0.0);
        QString label = QString::fromStdString(speaker.value("label", ""));
        
        m_speakerPositions.append(QPointF(x, y));
        m_speakerLabels.append(label);
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
  
  // Draw dimension labels
  painter.setPen(QColor(80, 80, 80));
  QFont font = painter.font();
  font.setPointSize(9);
  painter.setFont(font);
  
  // Width label
  QString widthText = QString("%1 m").arg(m_width, 0, 'f', 1);
  QRectF widthRect(m_zoneRect.left(), m_zoneRect.bottom() + 5, m_zoneRect.width(), 20);
  painter.drawText(widthRect, Qt::AlignCenter, widthText);
  
  // Height label
  painter.save();
  painter.translate(m_zoneRect.left() - 10, m_zoneRect.center().y());
  painter.rotate(-90);
  QString heightText = QString("%1 m").arg(m_height, 0, 'f', 1);
  painter.drawText(QRectF(-50, -10, 100, 20), Qt::AlignCenter, heightText);
  painter.restore();
  
  // Draw grid lines (optional, subtle)
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
  if(m_speakerPositions.size() == m_speakerLabels.size())
  {
    for(int i = 0; i < m_speakerPositions.size(); i++)
    {
      QPointF screenPos = worldToScreen(m_speakerPositions[i].x(), m_speakerPositions[i].y());
      
      // Draw speaker circle
      painter.setPen(QPen(QColor(0, 120, 215), 2));
      painter.setBrush(QColor(0, 120, 215, 100));
      painter.drawEllipse(screenPos, SPEAKER_RADIUS, SPEAKER_RADIUS);
      
      // Draw speaker number
      painter.setPen(QColor(255, 255, 255));
      QFont boldFont = font;
      boldFont.setBold(true);
      boldFont.setPointSize(8);
      painter.setFont(boldFont);
      painter.drawText(QRectF(screenPos.x() - 10, screenPos.y() - 10, 20, 20), 
                       Qt::AlignCenter, QString::number(i + 1));
      
      // Draw speaker label
      painter.setPen(QColor(60, 60, 60));
      painter.setFont(font);
      QRectF labelRect(screenPos.x() - 60, screenPos.y() + SPEAKER_RADIUS + 5, 120, 20);
      painter.drawText(labelRect, Qt::AlignCenter, m_speakerLabels[i]);
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
}
