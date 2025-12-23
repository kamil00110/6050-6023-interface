#ifndef TRAINTASTIC_CLIENT_SUBWINDOW_3DZONESUBWINDOW_HPP
#define TRAINTASTIC_CLIENT_SUBWINDOW_3DZONESUBWINDOW_HPP

#include "subwindow.hpp"

class ThreeDZoneEditorWidget;

class ThreeDZoneSubWindow : public SubWindow
{
  Q_OBJECT

protected:
  ThreeDZoneSubWindow(const ObjectPtr& object, QWidget* parent = nullptr);
  ThreeDZoneSubWindow(const std::shared_ptr<Connection>& connection, const QString& id, QWidget* parent = nullptr);

public:
  static ThreeDZoneSubWindow* create(const ObjectPtr& object, QWidget* parent = nullptr);
  static ThreeDZoneSubWindow* create(const std::shared_ptr<Connection>& connection, const QString& id, QWidget* parent = nullptr);

  // Remove 'final' and just override if it exists in base, or remove if not virtual
 
  SubWindowType type() override { return SubWindowType::ThreeDZone; }


protected:
  // Remove 'final' - check if these are virtual in SubWindow base class
  void objectChanged() override;
  void buildWidget() override;

private:
  ThreeDZoneEditorWidget* m_editor;
};

#endif
