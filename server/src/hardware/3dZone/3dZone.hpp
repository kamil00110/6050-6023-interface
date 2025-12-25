/**
 * server/src/hardware/3dZone/3dZone.hpp
 */
#ifndef TRAINTASTIC_SERVER_HARDWARE_3DZONE_3DZONE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_3DZONE_3DZONE_HPP

#include "../../core/idobject.hpp"
#include "../../core/property.hpp"
#include "../../core/method.hpp"
#include <traintastic/enum/speakersetup.hpp>

class ThreeDZone : public IdObject
{
  CLASS_ID("3d_zone")
  DEFAULT_ID("zone")
  CREATE(ThreeDZone)
  
  private:
    void updateEnabled();
    void refreshAudioDevices();  // Helper function
    
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
    Property<std::string> audioDevicesJson;  // Cached audio devices as JSON
    
    Method<void()> refreshAudioDevicesList;  // Method to refresh the list
    Method<void(std::string)> testSoundAtPosition;  // Change to accept single string
    Method<void(double, double, std::string)> playSoundAtPosition;  // x, y, soundId
   

    ThreeDZone(World& world, std::string_view _id);
};  // ADD SEMICOLON HERE

#endif
