#ifndef TRAINTASTIC_SERVER_HARDWARE_3DZONE_3DZONE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_3DZONE_3DZONE_HPP

#include "../../core/idobject.hpp"
#include "../../core/property.hpp"
#include "../../core/method.hpp"  // Add this
#include <traintastic/enum/speakersetup.hpp>

class ThreeDZone : public IdObject
{
  CLASS_ID("3d_zone")
  DEFAULT_ID("zone")
  CREATE(ThreeDZone)
  
  private:
    void updateEnabled();
    
  protected:
    void addToWorld() override;
    void loaded() override;
    void destroying() override;
    void worldEvent(WorldState state, WorldEvent event) override;
    
  public:
    Property<double> width;
    Property<double> height;
    Property<SpeakerSetup> speakerSetup;
    Property<std::string> speakersData;
    
    // Add this method for opening the editor
    Method<void()> openEditor;
    
    ThreeDZone(World& world, std::string_view _id);
};

#endif
