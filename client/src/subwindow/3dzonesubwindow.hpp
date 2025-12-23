#ifndef TRAINTASTIC_CLIENT_SUBWINDOW_3DZONESUBWINDOW_HPP
#define TRAINTASTIC_CLIENT_SUBWINDOW_3DZONESUBWINDOW_HPP

#include "subwindow.hpp"

class ThreeDZoneEditorWidget;

class ThreeDZoneSubWindow : public SubWindow
{
  Q_OBJECT

protected:
  ThreeDZoneSubWindow(const ObjectPtr& object, QWidget* parent = nullptr);
  ThreeDZoneSubWindow(const std::shared_ptr<Connection>& connection,
                      const QString& id,
                      QWidget* parent = nullptr);

  QWidget* createWidget(const ObjectPtr& object) override;

public:
  static ThreeDZoneSubWindow* create(const ObjectPtr& object, QWidget* parent = nullptr);
  static ThreeDZoneSubWindow* create(const std::shared_ptr<Connection>& connection,
                                     const QString& id,
                                     QWidget* parent = nullptr);

private:
  ThreeDZoneEditorWidget* m_editor = nullptr;
};


#endif
