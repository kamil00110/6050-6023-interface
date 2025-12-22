#ifndef TRAINTASTIC_SERVER_HARDWARE_3DZONE_3DZONE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_3DZONE_3DZONE_HPP

#include "../../core/idobject.hpp"
#include "../../core/property.hpp"
#include <traintastic/enum/speakersetup.hpp>
#include <array>

struct SpeakerConfiguration
{
  double volumeOverride = 1.0;
  std::string audioDevice;
  int audioChannel = 0;
};

class ThreeDZone : public IdObject, public std::enable_shared_from_this<ThreeDZone>
{

  CLASS_ID("3d_zone")
  DEFAULT_ID("zone")
  CREATE(ThreeDZone)
  
  private:
    void updateEnabled();
    void syncSpeakerData();
    
  protected:
    void addToWorld() override;
    void loaded() override;
    void destroying() override;
    void worldEvent(WorldState state, WorldEvent event) override;
    
  public:
    Property<double> width;
    Property<double> height;
    Property<SpeakerSetup> speakerSetup;
    
    // Speaker properties (0-9)
    Property<double> speaker0VolumeOverride;
    Property<std::string> speaker0AudioDevice;
    Property<int> speaker0AudioChannel;
    
    Property<double> speaker1VolumeOverride;
    Property<std::string> speaker1AudioDevice;
    Property<int> speaker1AudioChannel;
    
    Property<double> speaker2VolumeOverride;
    Property<std::string> speaker2AudioDevice;
    Property<int> speaker2AudioChannel;
    
    Property<double> speaker3VolumeOverride;
    Property<std::string> speaker3AudioDevice;
    Property<int> speaker3AudioChannel;
    
    Property<double> speaker4VolumeOverride;
    Property<std::string> speaker4AudioDevice;
    Property<int> speaker4AudioChannel;
    
    Property<double> speaker5VolumeOverride;
    Property<std::string> speaker5AudioDevice;
    Property<int> speaker5AudioChannel;
    
    Property<double> speaker6VolumeOverride;
    Property<std::string> speaker6AudioDevice;
    Property<int> speaker6AudioChannel;
    
    Property<double> speaker7VolumeOverride;
    Property<std::string> speaker7AudioDevice;
    Property<int> speaker7AudioChannel;
    
    Property<double> speaker8VolumeOverride;
    Property<std::string> speaker8AudioDevice;
    Property<int> speaker8AudioChannel;
    
    Property<double> speaker9VolumeOverride;
    Property<std::string> speaker9AudioDevice;
    Property<int> speaker9AudioChannel;
    
    ThreeDZone(World& world, std::string_view _id);
    
    // Helper to get speaker config
    SpeakerConfiguration getSpeaker(int index) const;
};

#endif
