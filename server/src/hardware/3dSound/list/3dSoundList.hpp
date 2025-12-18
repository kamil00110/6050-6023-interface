#ifndef TRAINTASTIC_SERVER_HARDWARE_3DSOUND_LIST_3DSOUNDLIST_HPP
#define TRAINTASTIC_SERVER_HARDWARE_3DSOUND_LIST_3DSOUNDLIST_HPP

#include "../../../core/objectlist.hpp"
#include "../3dSound.hpp"

class 3dSoundList : public ObjectList<3dSound>
{
public:
    3dSoundList(Object& parent, std::string_view parentPropertyName);

    TableModelPtr getModel();

    void worldEvent(WorldState state, WorldEvent event) override;

protected:
    bool isListedProperty(std::string_view name) override;
};

#endif
