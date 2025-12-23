#ifndef TRAINTASTIC_CLIENT_SUBWINDOW_3DZONE_SUBWINDOW_HPP
#define TRAINTASTIC_CLIENT_SUBWINDOW_3DZONE_SUBWINDOW_HPP

#include "subwindow.hpp"
#include "../network/object.hpp"

class ThreeDZoneSubWindow final : public SubWindow
{
  Q_OBJECT

  public:
    static ThreeDZoneSubWindow* create(const ObjectPtr& object)
    {
      if(!object)
        return nullptr;

      return new ThreeDZoneSubWindow(
        object->connection(),
        object->getProperty("id")->toString()
      );
    }

    static ThreeDZoneSubWindow* create(
      std::shared_ptr<Connection> connection,
      const QString& id)
    {
      return new ThreeDZoneSubWindow(std::move(connection), id);
    }

  protected:
    explicit ThreeDZoneSubWindow(
      std::shared_ptr<Connection> connection,
      const QString& id,
      QWidget* parent = nullptr)
      : SubWindow(SubWindowType::ThreeDZone, std::move(connection), id, parent)
    {
    }

    QWidget* createWidget(const ObjectPtr& object) override;
};

#endif
