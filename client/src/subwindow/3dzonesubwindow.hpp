#ifndef TRAINTASTIC_CLIENT_SUBWINDOW_3DZONESUBWINDOW_HPP
#define TRAINTASTIC_CLIENT_SUBWINDOW_3DZONESUBWINDOW_HPP

#include "subwindow.hpp"
#include <memory>

class ThreeDZoneEditorWidget;
class Object;

using ObjectPtr = std::shared_ptr<Object>;

class ThreeDZoneSubWindow : public SubWindow
{
    Q_OBJECT

public:
    static ThreeDZoneSubWindow* create(const ObjectPtr& object, QWidget* parent = nullptr);
    static ThreeDZoneSubWindow* create(const std::shared_ptr<Connection>& connection, const QString& id, QWidget* parent = nullptr);

protected:
    // Only keep constructors that match the base
    explicit ThreeDZoneSubWindow(SubWindowType type, QWidget* parent = nullptr);
    explicit ThreeDZoneSubWindow(SubWindowType type, std::shared_ptr<Connection> connection, const QString& id, QWidget* parent = nullptr);

    // implement pure virtual
    QWidget* createWidget(const ObjectPtr& object) override;

    void objectChanged();
    void buildWidget();

private:
    ObjectPtr m_object;                         // needed for your code
    ThreeDZoneEditorWidget* m_editor = nullptr;
};

#endif
