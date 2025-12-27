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
    void refreshAudioDevices();
    
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
    Property<std::string> audioDevicesJson;
    
    Method<void()> refreshAudioDevicesList;
    Method<void(std::string)> testSoundAtPosition;
    Method<void(double, double, std::string, std::string)> playSoundAtPosition; // x, y, instanceId, soundId
    Method<void(std::string, double, double)> moveSoundToPosition; // instanceId, newX, newY
    Method<void(std::string, double)> updateSoundVolume; // instanceId, newVolume
    Method<void(std::string)> stopSoundInstance; // instanceId
    Method<std::string()> getPlayingSounds; // Returns JSON list of playing sounds
   
    ThreeDZone(World& world, std::string_view _id);
};

#endif
