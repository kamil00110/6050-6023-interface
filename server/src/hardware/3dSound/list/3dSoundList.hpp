#ifndef TRAINTASTIC_SERVER_HARDWARE_3DSOUND_LIST_3DSOUNDLIST_HPP
#define TRAINTASTIC_SERVER_HARDWARE_3DSOUND_LIST_3DSOUNDLIST_HPP

#include "../../../core/objectlist.hpp"   // Correct path from 'list' folder
#include "../3dSound.hpp"                  // The 3dSound object itself

template<typename T>
class ObjectListTableModel;              // Only needed if we ever use table models

class 3dSoundList : public ObjectList<3dSound>
{
  public:
    Method<3dSoundList, std::shared_ptr<3dSound>> create;

    explicit 3dSoundList(Object& parent, std::string_view parentPropertyName);

    bool isListedProperty(std::string_view name) override;

    void worldEvent(WorldState state, WorldEvent event) override;
};

#endif
