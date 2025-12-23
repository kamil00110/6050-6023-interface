#include "3dzonesubwindow.hpp"
#include "../widget/3dZone/3dzoneeditorwidget.hpp"
#include "../network/object.hpp"
#include "../network/property.hpp"
#include <QVBoxLayout>

ThreeDZoneSubWindow::ThreeDZoneSubWindow(const ObjectPtr& object, QWidget* parent)
  : SubWindow(object, parent)
  , m_editor{nullptr}
{
  buildWidget();
}

ThreeDZoneSubWindow::ThreeDZoneSubWindow(const std::shared_ptr<Connection>& connection, const QString& id, QWidget* parent)
  : SubWindow(connection, id, parent)
  , m_editor{nullptr}
{
}

ThreeDZoneSubWindow* ThreeDZoneSubWindow::create(const ObjectPtr& object, QWidget* parent)
{
  return new ThreeDZoneSubWindow(object, parent);
}

ThreeDZoneSubWindow* ThreeDZoneSubWindow::create(const std::shared_ptr<Connection>& connection, const QString& id, QWidget* parent)
{
  return new ThreeDZoneSubWindow(connection, id, parent);
}

void ThreeDZoneSubWindow::objectChanged()
{
  SubWindow::objectChanged();
  
  if(m_object)
  {
    if(auto* idProp = m_object->getProperty("id"))
    {
      setWindowTitle(QString("3D Zone Editor - %1").arg(idProp->toString()));
    }
    else
    {
      setWindowTitle("3D Zone Editor");
    }
  }
  
  buildWidget();
}

void ThreeDZoneSubWindow::buildWidget()
{
  if(!m_object)
    return;
    
  if(m_editor)
  {
    delete m_editor;
    m_editor = nullptr;
  }
  
  m_editor = new ThreeDZoneEditorWidget(m_object, this);
  
  QVBoxLayout* layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_editor);
  
  if(QWidget* w = widget())
    delete w;
    
  QWidget* container = new QWidget(this);
  container->setLayout(layout);
  setWidget(container);
}
