#ifndef TRAINTASTIC_SERVER_HARDWARE_3DZONE_3DZONE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_3DZONE_3DZONE_HPP

#include "../../core/idobject.hpp"
#include "../../core/property.hpp"
#include <traintastic/enum/speakersetup.hpp>
#include <nlohmann/json.hpp>

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
    Property<std::string> speakersData;  // JSON string containing all speaker configurations
    
    ThreeDZone(World& world, std::string_view _id);
    
    // Helper to parse speaker data
    nlohmann::json getSpeakersJson() const;
    std::string getSpeakersFormatted() const;
};

#endif
