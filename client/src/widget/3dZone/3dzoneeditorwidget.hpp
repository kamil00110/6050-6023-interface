#ifndef TRAINTASTIC_CLIENT_WIDGET_3DZONE_3DZONEEDITORWIDGET_HPP
#define TRAINTASTIC_CLIENT_WIDGET_3DZONE_3DZONEEDITORWIDGET_HPP

#include <QWidget>
#include <QDialog>
#include "../../network/object.hpp"
#include "../../network/objectptr.hpp"

class QPaintEvent;
class QLabel;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;

struct AudioDeviceData {
  QString deviceId;
  QString deviceName;
  int channelCount;
  bool isDefault;
  
  struct ChannelData {
    int index;
    QString name;
  };
  std::vector<ChannelData> channels;
};

// Speaker configuration dialog
class SpeakerConfigDialog : public QDialog
{
  Q_OBJECT

public:
  SpeakerConfigDialog(int speakerId, const QString& label, 
                      const QString& device, int channel, double volume,
                      const std::vector<AudioDeviceData>& audioDevices,
                      QWidget* parent = nullptr);
  
  QString getDevice() const;
  int getChannel() const;
  double getVolume() const;

private slots:
  void onDeviceChanged(int index);

private:
  QComboBox* m_deviceCombo;
  QComboBox* m_channelCombo;
  QDoubleSpinBox* m_volumeSpin;
  QPushButton* m_testButton;
  
  const std::vector<AudioDeviceData>& m_audioDevices;
};

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
  void mousePressEvent(QMouseEvent* event) override;

private slots:
  void updateFromProperties();
  void updateSpeakers();
  void loadAudioDevices();

private:
  ObjectPtr m_zone;
  double m_width;   // in cm
  double m_height;  // in cm
  int m_speakerCount;
  
  struct SpeakerInfo {
    int id;
    QPointF position;  // in cm
    QString label;
    QString device;
    int channel;
    double volume;
  };
  
  QVector<SpeakerInfo> m_speakers;
  std::vector<AudioDeviceData> m_audioDevices;
  int m_selectedSpeaker;
  
  QRectF m_zoneRect;
  double m_scale;
  
  void calculateLayout();
  void calculateSpeakerPositions();
  QPointF worldToScreen(double x, double y) const;
  int getSpeakerAtPosition(const QPointF& pos) const;
  void openSpeakerConfig(int speakerId);
  void saveSpeakersToProperty();
  QString getDeviceDisplayName(const QString& deviceId) const;
};

#endif
