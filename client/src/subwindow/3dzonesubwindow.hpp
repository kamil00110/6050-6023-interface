#ifndef TRAINTASTIC_CLIENT_SUBWINDOW_3DZONE_SUBWINDOW_HPP
#define TRAINTASTIC_CLIENT_SUBWINDOW_3DZONE_SUBWINDOW_HPP

#include "subwindow.hpp"

class ThreeDZoneEditorWidget;
class Object;

class ThreeDZoneSubWindow final : public SubWindow
{
    Q_OBJECT

public:
    static ThreeDZoneSubWindow* create(const ObjectPtr& object, QWidget* parent = nullptr);
    static ThreeDZoneSubWindow* create(std::shared_ptr<Connection> connection, const QString& id, QWidget* parent = nullptr);

protected:
    explicit ThreeDZoneSubWindow(std::shared_ptr<Connection> connection, const QString& id, QWidget* parent = nullptr);

    // Implementation of pure virtual
    QWidget* createWidget(const ObjectPtr& object) override;

private:
    ObjectPtr m_object;
    ThreeDZoneEditorWidget* m_editor = nullptr;

    void objectChanged();
    void buildWidget();
};

#endif
