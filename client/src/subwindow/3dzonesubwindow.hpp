#ifndef TRAINTASTIC_CLIENT_SUBWINDOW_3DZONE_SUBWINDOW_HPP
#define TRAINTASTIC_CLIENT_SUBWINDOW_3DZONE_SUBWINDOW_HPP

#include "subwindow.hpp"

class ThreeDZoneSubWindow : public SubWindow
{
  Q_OBJECT

  public:
    explicit ThreeDZoneSubWindow(
      std::shared_ptr<Connection> connection,
      const QString& id,
      QWidget* parent = nullptr)
      : SubWindow(SubWindowType::ThreeDZone, std::move(connection), id, parent)
    {
    }

  protected:
    QWidget* createWidget(const ObjectPtr& object) override;
    QSize defaultSize() const override { return QSize(600, 400); }
};

#endif
