#ifndef TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_SPEAKERSETUP_HPP
#define TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_SPEAKERSETUP_HPP

#include <cstdint>

enum class SpeakerSetup : uint8_t
{
  Quadraphonic = 4,    // 4 speakers
  Surround5_1 = 6,     // 6 speakers
  Surround7_1 = 8,     // 8 speakers
  Surround9_1 = 10,    // 10 speakers
};

TRAINTASTIC_ENUM(SpeakerSetup, "speaker_setup", 4,
{
  {SpeakerSetup::Quadraphonic, "quadraphonic"},
  {SpeakerSetup::Surround5_1, "surround_5_1"},
  {SpeakerSetup::Surround7_1, "surround_7_1"},
  {SpeakerSetup::Surround9_1, "surround_9_1"},
});

#endif
