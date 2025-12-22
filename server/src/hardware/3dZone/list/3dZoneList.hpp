/**
 * server/src/hardware/3dZone/list/3dZoneList.hpp
 */
#ifndef TRAINTASTIC_SERVER_HARDWARE_3DZONE_LIST_3DZONELIST_HPP
#define TRAINTASTIC_SERVER_HARDWARE_3DZONE_LIST_3DZONELIST_HPP

#include "../../../core/objectlist.hpp"
#include "3dZoneListColumn.hpp"
#include "../../../core/method.hpp"

class ThreeDZone;

class ThreeDZoneList : public ObjectList<ThreeDZone>
{
  CLASS_ID("list.3d_zone")
  
  protected:
    void worldEvent(WorldState state, WorldEvent event) final;
    bool isListedProperty(std::string_view name) final;
    
  public:
    const ThreeDZoneListColumn columns;
    
    Method<std::shared_ptr<ThreeDZone>()> create;
    Method<void(const std::shared_ptr<ThreeDZone>&)> delete_;
    
    ThreeDZoneList(Object& _parent, std::string_view parentPropertyName, ThreeDZoneListColumn _columns);
    
    TableModelPtr getModel() final;
};

#endif
