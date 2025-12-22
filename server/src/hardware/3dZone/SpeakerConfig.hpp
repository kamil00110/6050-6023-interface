#ifndef TRAINTASTIC_SERVER_HARDWARE_3DZONE_SPEAKERCONFIG_HPP
#define TRAINTASTIC_SERVER_HARDWARE_3DZONE_SPEAKERCONFIG_HPP

#include "../../core/subobject.hpp"
#include "../../core/property.hpp"

class ThreeDZone;

class SpeakerConfig : public SubObject
{
  CLASS_ID("speaker_config")
  
  private:
    int m_speakerIndex;
    void updateEnabled();
    
  protected:
    void worldEvent(WorldState state, WorldEvent event) override;
    
  public:
    Property<double> volumeOverride;
    Property<std::string> audioDevice;
    Property<int> audioChannel;
    
    SpeakerConfig(ThreeDZone& parent, int speakerIndex);
    
    int speakerIndex() const { return m_speakerIndex; }
};

#endif
