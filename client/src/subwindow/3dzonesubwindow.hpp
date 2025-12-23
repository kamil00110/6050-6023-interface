#ifndef TRAINTASTIC_CLIENT_SUBWINDOW_3DZONE_SUBWINDOW_HPP
#define TRAINTASTIC_CLIENT_SUBWINDOW_3DZONE_SUBWINDOW_HPP

#include "subwindow.hpp"
#include <memory>

class ThreeDZoneEditorWidget;
class Object;

using ObjectPtr = std::shared_ptr<Object>;

class ThreeDZoneSubWindow final : public SubWindow
{
    Q_OBJECT

public:
    // Factory methods
    static ThreeDZoneSubWindow* create(const ObjectPtr& object, QWidget* parent = nullptr);
    static ThreeDZoneSubWindow* create(std::shared_ptr<Connection> connection, const QString& id, QWidget* parent = nullptr);

protected:
    explicit ThreeDZoneSubWindow(std::shared_ptr<Connection> connection, const QString& id, QWidget* parent = nullptr);

    // Implement pure virtual from SubWindow
    QWidget* createWidget(const ObjectPtr& object) override;

private:
    ObjectPtr m_object;                         // store the object
    ThreeDZoneEditorWidget* m_editor = nullptr; // pointer to editor widget

    void objectChanged(); // mark as private, not override
    void buildWidget();
};

#endif
