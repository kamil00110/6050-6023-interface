/**
 * client/src/widget/createwidget.cpp
 *
 * This file is part of the traintastic source code.
 *
 * Copyright (C) 2020-2025 Reinder Feenstra
 */

#include "createwidget.hpp"
#include "list/marklincanlocomotivelistwidget.hpp"
#include "objectlist/boardlistwidget.hpp"
#include "objectlist/interfacelistwidget.hpp"
#include "objectlist/throttleobjectlistwidget.hpp"
#include "objectlist/trainlistwidget.hpp"
#include "objectlist/zoneblocklistwidget.hpp"
#include "object/luascripteditwidget.hpp"
#include "object/objecteditwidget.hpp"
#include "object/itemseditwidget.hpp"
#include "tile/tilewidget.hpp"
#include "camera/cameraeditwidget.hpp"          // ← new
#include "inputmonitorwidget.hpp"
#include "outputkeyboardwidget.hpp"
#include "outputmapwidget.hpp"
#include "propertycheckbox.hpp"
#include "propertycombobox.hpp"
#include "propertydoublespinbox.hpp"
#include "propertyspinbox.hpp"
#include "propertylineedit.hpp"
#include "propertypairoutputaction.hpp"
#include "propertyvaluelabel.hpp"
#include "objectpropertycombobox.hpp"
#include "objectnamelabel.hpp"
#include "../board/boardwidget.hpp"
#include "../network/object.hpp"
#include "../network/inputmonitor.hpp"
#include "../network/outputkeyboard.hpp"
#include "../network/board.hpp"
#include "../network/property.hpp"
#include "../network/objectproperty.hpp"

QWidget* createWidgetIfCustom(const ObjectPtr& object, QWidget* parent)
{
  const QString& classId = object->classId();

  if(classId == "camera")                        // ← new
    return new CameraEditWidget(object, parent); // ← new

  if(classId == "list.interface")
    return new InterfaceListWidget(object, parent);
  else if(classId == "controller_list")
    return new ObjectListWidget(object, parent);
  else if(classId == "rail_vehicle_list")
    return new ObjectListWidget(object, parent);
  else if(classId == "lua.script_list")
    return new ObjectListWidget(object, parent);
  else if(classId == "world_list")
    return new ObjectListWidget(object, parent);
  if(classId == "list.board")
    return new BoardListWidget(object, parent);
  if(classId == "list.train")
    return new TrainListWidget(object, parent);
  if(classId == "list.zone_block")
    return new ZoneBlockListWidget(object, parent);
  else if(object->classId().startsWith("list."))
    return new ObjectListWidget(object, parent);
  else if(classId == "lua.script")
    return new LuaScriptEditWidget(object, parent);
  else if(classId.startsWith("output_map."))
    return new OutputMapWidget(object, parent);
  else if(classId == "input_map.block" || classId == "decoder_functions")
    return new ItemsEditWidget(object, parent);
  else if(classId == "marklin_can_node_list")
    return new ListWidget(object, parent);
  else if(classId == "marklin_can_locomotive_list")
    return new MarklinCANLocomotiveListWidget(object, parent);
  else
    return nullptr;
}

QWidget* createWidget(const ObjectPtr& object, QWidget* parent)
{
  if(QWidget* widget = createWidgetIfCustom(object, parent))
    return widget;
  else if(auto inputMonitor = std::dynamic_pointer_cast<InputMonitor>(object))
    return new InputMonitorWidget(inputMonitor, parent);
  else if(auto outputKeyboard = std::dynamic_pointer_cast<OutputKeyboard>(object))
    return new OutputKeyboardWidget(outputKeyboard, parent);
  else if(object->classId().startsWith("board_tile."))
    return new TileWidget(object, parent);
  else if(object->classId() == "booster")
    return new TileWidget(object, parent);
  else
    return new ObjectEditWidget(object, parent);
}

QWidget* createWidget(InterfaceItem& item, QWidget* parent)
{
  if(auto* baseProperty = dynamic_cast<AbstractProperty*>(&item))
    return createWidget(*baseProperty, parent);
  assert(false);
  return nullptr;
}

QWidget* createWidget(AbstractProperty& baseProperty, QWidget* parent)
{
  if(auto* property = dynamic_cast<Property*>(&baseProperty))
    return createWidget(*property, parent);
  else if(auto* objectProperty = dynamic_cast<ObjectProperty*>(&baseProperty))
    return createWidget(*objectProperty, parent);
  assert(false);
  return nullptr;
}

QWidget* createWidget(Property& property, QWidget* parent)
{
  if(!property.isWritable())
    return new PropertyValueLabel(property, parent);

  switch(property.type())
  {
    case ValueType::Boolean:
      return new PropertyCheckBox(property, parent);

    case ValueType::Enum:
      if(property.enumName() == "pair_output_action")
        return new PropertyPairOutputAction(property, parent);
      return new PropertyComboBox(property, parent);

    case ValueType::Integer:
      if(property.hasAttribute(AttributeName::Values) && !property.hasAttribute(AttributeName::Min) && !property.hasAttribute(AttributeName::Max))
        return new PropertyComboBox(property, parent);
      return new PropertySpinBox(property, parent);

    case ValueType::Float:
      return new PropertyDoubleSpinBox(property, parent);

    case ValueType::String:
      if(property.hasAttribute(AttributeName::Values))
        return new PropertyComboBox(property, parent);
      return new PropertyLineEdit(property, parent);

    case ValueType::Object:
      break;

    case ValueType::Set:
      break;

    case ValueType::Invalid:
      break;
  }
  assert(false);
  return nullptr;
}

QWidget* createWidget(ObjectProperty& property, QWidget* parent)
{
  if(property.isWritable())
    return new ObjectPropertyComboBox(property, parent);
  return new ObjectNameLabel(property, parent);
}
