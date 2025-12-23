/**
 * client/src/widget/object/objecteditwidget.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2019-2025 Reinder Feenstra
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "objecteditwidget.hpp"

#include <QFile>
#include <QMessageBox>
#include <QProgressDialog>

#include <QFormLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QPushButton>
#include "../../network/connection.hpp"
#include "../../network/error.hpp"
#include "../../network/object.hpp"
#include "../../network/property.hpp"
#include "../../network/objectproperty.hpp"
#include "../../network/unitproperty.hpp"
#include "../../network/method.hpp"
#include "../interfaceitemnamelabel.hpp"
#include "../propertycheckbox.hpp"
#include "../propertycombobox.hpp"
#include "../objectpropertycombobox.hpp"
#include "../propertyspinbox.hpp"
#include "../propertydoublespinbox.hpp"
#include "../propertylineedit.hpp"
#include "../propertytextedit.hpp"
#include "../propertyobjectedit.hpp"
#include "../propertydirectioncontrol.hpp"
#include "../propertyvaluelabel.hpp"
#include "../methodpushbutton.hpp"
#include "../unitpropertycombobox.hpp"
#include "../unitpropertyedit.hpp"
#include "../createwidget.hpp"
#include "../decoder/decoderwidget.hpp"
#include "../decoder/decoderfunctionswidget.hpp"
#include "../../theme/theme.hpp"
#include "../3dzone/3dzoneeditorwidget.hpp"
#include <traintastic/enum/direction.hpp>
#include <traintastic/locale/locale.hpp>

static QWidget* createFilePickerWidget(Property& property, QWidget* parent)
{
  QWidget* container = new QWidget(parent);
  QHBoxLayout* layout = new QHBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  
  PropertyLineEdit* lineEdit = new PropertyLineEdit(property, container);
  lineEdit->setReadOnly(true); // Make read-only since we upload files
  
  QPushButton* browseButton = new QPushButton("...", container);
  browseButton->setMaximumWidth(30);
  browseButton->setToolTip(QObject::tr("Browse and upload audio file"));
  
  // Update button state when property changes
  QObject::connect(&property, &Property::valueChanged, browseButton,
    [browseButton, &property]()
    {
      // Optionally disable browse if file is already set
      // browseButton->setEnabled(property.toString().isEmpty());
    });
  
  // Browse and upload functionality
  QObject::connect(browseButton, &QPushButton::clicked, container,
    [&property, lineEdit, parent]()
    {
      QString startPath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
      
      QString filename = QFileDialog::getOpenFileName(
        lineEdit,
        QObject::tr("Select Audio File"),
        startPath,
        QObject::tr("Audio Files (*.wav *.mp3 *.ogg *.flac *.aac *.m4a);;All Files (*)"));
      
      if(!filename.isEmpty())
      {
        QFile file(filename);
        if(!file.open(QIODevice::ReadOnly))
        {
          QMessageBox::critical(parent, 
            QObject::tr("Error"), 
            QObject::tr("Failed to read file: %1").arg(filename));
          return;
        }
        
        QByteArray fileData = file.readAll();
        file.close();
        
        // Show progress dialog (heap allocated, will be deleted when closed)
        QProgressDialog* progress = new QProgressDialog(
          QObject::tr("Uploading audio file..."), 
          QString(), 0, 100, parent);
        progress->setWindowModality(Qt::WindowModal);
        progress->setCancelButton(nullptr); // No cancel button
        progress->setAutoClose(true);
        progress->setAutoReset(true);
        progress->setValue(50);
        progress->show();
        
        // Get the object from the property's parent
        Object* obj = static_cast<Object*>(property.parent());
        if(!obj)
        {
          QMessageBox::critical(parent,
            QObject::tr("Error"),
            QObject::tr("Failed to get object"));
          return;
        }
        
        // Get the upload method
        Method* uploadMethod = obj->getMethod("upload_audio_file");
        
        if(uploadMethod)
        {
          Connection* connection = obj->connection().get();
          QFileInfo fileInfo(filename);
          
          QStringList args;
          args << fileInfo.fileName(); // Original filename
          
          connection->callMethodWithBinaryData(*uploadMethod, args, fileData,
            [progress, parent](std::optional<const Error> error)
            {
              // Close and delete progress dialog
              progress->setValue(100);
              progress->close();
              progress->deleteLater();
              
              if(error)
              {
                QMessageBox::critical(parent, 
                  QObject::tr("Upload Failed"), 
                  QObject::tr("Failed to upload audio file: %1").arg(error->toString()));
              }
              else
              {
                // Success - property will be updated by server
                QMessageBox::information(parent,
                  QObject::tr("Success"),
                  QObject::tr("Audio file uploaded successfully"));
              }
            });
        }
        else
        {
          QMessageBox::critical(parent,
            QObject::tr("Error"),
            QObject::tr("Upload method not available"));
        }
      }
    });
  
  // Handle enabled state
  QObject::connect(&property, &Property::attributeChanged, container,
    [browseButton, lineEdit](AttributeName name, const QVariant& value)
    {
      if(name == AttributeName::Enabled)
      {
        const bool enabled = value.toBool();
        lineEdit->setEnabled(enabled);
        browseButton->setEnabled(enabled);
      }
    });
  
  const bool enabled = property.getAttributeBool(AttributeName::Enabled, true);
  lineEdit->setEnabled(enabled);
  browseButton->setEnabled(enabled);
  
  layout->addWidget(lineEdit);
  layout->addWidget(browseButton);
  
  return container;
}
ObjectEditWidget::ObjectEditWidget(const ObjectPtr& object, QWidget* parent) :
  AbstractEditWidget(object, parent)
{
  buildForm();
}

ObjectEditWidget::ObjectEditWidget(const QString& id, QWidget* parent) :
  AbstractEditWidget(id, parent)
{
}

ObjectEditWidget::ObjectEditWidget(ObjectProperty& property, QWidget* parent)
  : AbstractEditWidget(property, parent)
{
}

void ObjectEditWidget::buildForm()
{
  setObjectWindowTitle();
  Theme::setWindowIcon(*this, m_object->classId());

  // NEW: Add special handling for 3D Zone editor
  if(m_object->classId() == "3d_zone")
  {
    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Add the visual editor at the top
    ThreeDZoneEditorWidget* zoneEditor = new ThreeDZoneEditorWidget(m_object, this);
    mainLayout->addWidget(zoneEditor);
    
    // Add a separator or some spacing
    mainLayout->addSpacing(10);
    
    // Create a collapsible section for properties (optional)
    // Or just add the properties directly below
    QWidget* propertiesWidget = new QWidget(this);
    QFormLayout* propsLayout = new QFormLayout(propertiesWidget);
    
    // Add the basic properties (width, height, speaker setup)
    for(const QString& name : m_object->interfaceItems().names())
    {
      if(InterfaceItem* item = m_object->getInterfaceItem(name))
      {
        if(!item->getAttributeBool(AttributeName::ObjectEditor, true))
          continue;
          
        // Only show specific properties, skip the hidden ones
        if(name == "speakers_data" || name == "open_editor")
          continue;
        
        QWidget* w = nullptr;
        
        if(AbstractProperty* baseProperty = dynamic_cast<AbstractProperty*>(item))
        {
          if(Property* property = dynamic_cast<Property*>(baseProperty))
          {
            w = createWidget(*property, this);
          }
        }
        else if(Method* method = dynamic_cast<Method*>(item))
        {
          // Skip methods for now, or add them if needed
          continue;
        }
        
        if(w)
        {
          propsLayout->addRow(new InterfaceItemNameLabel(*item, this), w);
        }
      }
    }
    
    mainLayout->addWidget(propertiesWidget);
    mainLayout->addStretch(); 
    
    setLayout(mainLayout);
    return; 
  }
  if(QWidget* widget = createWidgetIfCustom(m_object))
  {
    QVBoxLayout* l = new QVBoxLayout();
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(widget);
    setLayout(l);
  }
  else
  {
    QList<QWidget*> tabs;
    QMap<QString, QWidget*> categoryTabs;

    for(const QString& name : m_object->interfaceItems().names())
    {
      if(InterfaceItem* item = m_object->getInterfaceItem(name))
      {
        if(!item->getAttributeBool(AttributeName::ObjectEditor, true))
          continue;

        QWidget* w = nullptr;

        if(AbstractProperty* baseProperty = dynamic_cast<AbstractProperty*>(item))
        {
          if(baseProperty->type() == ValueType::Object)
          {
            ObjectProperty* property = static_cast<ObjectProperty*>(baseProperty);
            if(property->name() == "decoder")
            {
              tabs.append(new DecoderWidget(*property, this));
              tabs.append(new DecoderFunctionsWidget(*property, this));
              continue;
            }
            else if(contains(baseProperty->flags(), PropertyFlags::SubObject))
            {
              tabs.append(new ObjectEditWidget(*property, this));
              continue;
            }
            else if(property->name() == "interface")
            {
              w = new ObjectPropertyComboBox(*property, this);
            }
            else
            {
              w = new PropertyObjectEdit(*property, this);
            }
          }
          else
          {
            Property* property = static_cast<Property*>(baseProperty);
            if(UnitProperty* unitProperty = dynamic_cast<UnitProperty*>(property))
            {
              if(unitProperty->hasAttribute(AttributeName::Values))
              {
                w = new UnitPropertyComboBox(*unitProperty, this);
              }
              else
              {
                w = new UnitPropertyEdit(*unitProperty, this);
              }
            }
            else if(!property->isWritable())
              w = new PropertyValueLabel(*property, this);
            else if(property->type() == ValueType::Boolean)
              w = new PropertyCheckBox(*property, this);
            else if(property->type() == ValueType::Integer)
            {
              w = createWidget(*property, this);
            }
            else if(property->type() == ValueType::Float)
              w = createWidget(*property, this);
            else if(property->type() == ValueType::String)
{
  if(property->name() == "notes")
  {
    PropertyTextEdit* edit = new PropertyTextEdit(*property, this);
    edit->setWindowTitle(property->displayName());
    edit->setPlaceholderText(property->displayName());
    tabs.append(edit);
    continue;
  }
  else if(property->name() == "code")
  {
    PropertyTextEdit* edit = new PropertyTextEdit(*property, this);
    edit->setWindowTitle(property->displayName());
    edit->setPlaceholderText(property->displayName());
    tabs.append(edit);
    continue;
  }
  else if(property->name() == "sound_file")  // NEW: File picker for sound files
  {
    w = createFilePickerWidget(*property, this);
  }
  else
    w = createWidget(*property, this);
}
            else if(property->type() == ValueType::Enum)
            {
              if(property->enumName() == EnumName<Direction>::value)
                w = new PropertyDirectionControl(*property, this);
              else
                w = createWidget(*property, this);
            }
          }
        }
        else if(Method* method = dynamic_cast<Method*>(item))
        {
          w = new MethodPushButton(*method, this);
        }

        const QString category = item->getAttributeString(AttributeName::Category, "category:general");
        QWidget* tabWidget;
        if(!categoryTabs.contains(category))
        {
          tabWidget = new QWidget(this);
          tabWidget->setWindowTitle(Locale::tr(category));
          tabWidget->setLayout(new QFormLayout());
          tabs.append(tabWidget);
          categoryTabs.insert(category, tabWidget);
        }
        else
          tabWidget = categoryTabs[category];

        static_cast<QFormLayout*>(tabWidget->layout())->addRow(new InterfaceItemNameLabel(*item, this), w);
      }
    }

    if(tabs.count() > 1)
    {
      QTabWidget* tabWidget = new QTabWidget(this);
      for(auto* tab : tabs)
        tabWidget->addTab(tab, tab->windowTitle());
      QVBoxLayout* l = new QVBoxLayout();
      l->setContentsMargins(0, 0, 0, 0);
      l->addWidget(tabWidget);
      setLayout(l);
    }
    else if(tabs.count() == 1)
    {
      QWidget* w = tabs.first();
      setLayout(w->layout());
      delete w;
    }
  }
}
