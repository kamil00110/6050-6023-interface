/**
 * server/src/hardware/3dZone/list/3dZoneListTableModel.hpp
 */
#ifndef TRAINTASTIC_SERVER_HARDWARE_3DZONE_LIST_3DZONELISTTABLEMODEL_HPP
#define TRAINTASTIC_SERVER_HARDWARE_3DZONE_LIST_3DZONELISTTABLEMODEL_HPP

#include "../../../core/objectlisttablemodel.hpp"
#include "3dZoneListColumn.hpp"
#include "../3dZone.hpp"
#include "../network/object.hpp"  // Add this


class ThreeDZoneList;

class ThreeDZoneListTableModel : public ObjectListTableModel<ThreeDZone>
{
  friend class ThreeDZoneList;
  
  private:
    std::vector<ThreeDZoneListColumn> m_columns;
    void changed(uint32_t row, ThreeDZoneListColumn column);
    
  protected:
    void propertyChanged(BaseProperty& property, uint32_t row) final;
    
  public:
    CLASS_ID("3d_zone_list_table_model")
    
    static bool isListedProperty(std::string_view name);
    
    ThreeDZoneListTableModel(ThreeDZoneList& list);
    
    std::string getText(uint32_t column, uint32_t row) const final;
};

#endif
