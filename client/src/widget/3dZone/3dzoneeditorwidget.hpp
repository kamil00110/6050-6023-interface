#ifndef TRAINTASTIC_CLIENT_WIDGET_3DZONE_3DZONEEDITORWIDGET_HPP
#define TRAINTASTIC_CLIENT_WIDGET_3DZONE_3DZONEEDITORWIDGET_HPP

#include <QWidget>
#include "../../network/object.hpp"
#include "../../network/objectptr.hpp"

class QPaintEvent;
class QLabel;

class ThreeDZoneEditorWidget : public QWidget
{
  Q_OBJECT

public:
  explicit ThreeDZoneEditorWidget(const ObjectPtr& zone, QWidget* parent = nullptr);
  
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private slots:
  void updateFromProperties();
  void updateSpeakers();

private:
  ObjectPtr m_zone;
  double m_width;
  double m_height;
  int m_speakerCount;
  QVector<QPointF> m_speakerPositions;
  QVector<QString> m_speakerLabels;
  
  QRectF m_zoneRect;
  double m_scale;
  
  void calculateLayout();
  void calculateSpeakerPositions();
  QPointF worldToScreen(double x, double y) const;
};

#endif
