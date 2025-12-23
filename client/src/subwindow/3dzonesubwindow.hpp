#ifndef TRAINTASTIC_CLIENT_SUBWINDOW_3DZONE_SUBWINDOW_HPP
#define TRAINTASTIC_CLIENT_SUBWINDOW_3DZONE_SUBWINDOW_HPP

#include "subwindow.hpp"
#include "../network/object.hpp"   // Required for ObjectPtr usage

class ThreeDZoneSubWindow final : public SubWindow
{
  Q_OBJECT

public:
    static ThreeDZoneSubWindow* create(const ObjectPtr& object, QWidget* parent = nullptr);
    static ThreeDZoneSubWindow* create(std::shared_ptr<Connection> connection, const QString& id, QWidget* parent = nullptr);

protected:
    explicit ThreeDZoneSubWindow(const ObjectPtr& object, QWidget* parent = nullptr);
    explicit ThreeDZoneSubWindow(std::shared_ptr<Connection> connection, const QString& id, QWidget* parent = nullptr);

    void objectChanged() override;
    void buildWidget();
};

#endif

