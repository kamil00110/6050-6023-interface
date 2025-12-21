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
  
  QPushButton* clearButton = new QPushButton("X", container);
  clearButton->setMaximumWidth(30);
  clearButton->setToolTip(QObject::tr("Clear audio file"));
  clearButton->setEnabled(!property.toString().isEmpty());
  
  // Update clear button state when property changes
  QObject::connect(&property, &Property::valueChanged, clearButton,
    [clearButton, &property]()
    {
      clearButton->setEnabled(!property.toString().isEmpty());
    });
  
  // Clear button functionality
  QObject::connect(clearButton, &QPushButton::clicked, container,
    [&property]()
    {
      if(QMessageBox::question(nullptr, 
          QObject::tr("Clear Audio File"), 
          QObject::tr("Delete the audio file from the server?"),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
      {
        property.setValueString("");
      }
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
        
        // Show progress dialog
        QProgressDialog progress(QObject::tr("Uploading audio file..."), 
                                QObject::tr("Cancel"), 0, 100, parent);
        progress.setWindowModality(Qt::WindowModal);
        progress.setValue(50);
        
        // Get the upload method
        Method* uploadMethod = object.getMethod("upload_audio_file");
        qDebug() << "Upload method found:" << (uploadMethod != nullptr);
        
        if(uploadMethod)
        {
          Connection* connection = object.connection().get();
          QFileInfo fileInfo(filename);
          
          QStringList args;
          args << fileInfo.fileName(); // Original filename
          
          connection->callMethodWithBinaryData(*uploadMethod, args, fileData,
            [&progress, &property, fileInfo](std::optional<const Error> error)
            {
              progress.setValue(100);
              
              if(error)
              {
                QMessageBox::critical(nullptr, 
                  QObject::tr("Upload Failed"), 
                  QObject::tr("Failed to upload audio file: %1").arg(error->toString()));
              }
              else
              {
                // Success - property will be updated by server
                QMessageBox::information(nullptr,
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
  // In objecteditwidget.cpp, in the browse button callback:
QObject::connect(browseButton, &QPushButton::clicked, container,
  [&property, lineEdit, parent]()
  {
    qDebug() << "Browse button clicked"; // ADD THIS
    
    QString filename = QFileDialog::getOpenFileName(/*...*/);
    qDebug() << "Selected file:" << filename; // ADD THIS
    
    if(!filename.isEmpty())
    {
      QFile file(filename);
      if(!file.open(QIODevice::ReadOnly))
      {
        qDebug() << "Failed to open file:" << filename; // ADD THIS
        // ...
      }
      
      QByteArray fileData = file.readAll();
      qDebug() << "Read" << fileData.size() << "bytes"; // ADD THIS
      
      // ...
    }
  });
  
  const bool enabled = property.getAttributeBool(AttributeName::Enabled, true);
  lineEdit->setEnabled(enabled);
  browseButton->setEnabled(enabled);
  clearButton->setEnabled(enabled && !property.toString().isEmpty());
  
  layout->addWidget(lineEdit);
  layout->addWidget(browseButton);
  layout->addWidget(clearButton);
  
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
