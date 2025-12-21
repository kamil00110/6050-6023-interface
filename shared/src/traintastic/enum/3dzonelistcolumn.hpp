#ifndef TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_3DZONELISTCOLUMN_HPP
#define TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_3DZONELISTCOLUMN_HPP

#include <cstdint>
#include <array>

enum class ThreeDZoneListColumn : uint8_t
{
  Id = 1,
  Width = 2,
  Height = 4,
  SpeakerSetup = 8,
};

TRAINTASTIC_ENUM(ThreeDZoneListColumn, "3d_zone_list_column", 4,
{
  {ThreeDZoneListColumn::Id, "id"},
  {ThreeDZoneListColumn::Width, "width"},
  {ThreeDZoneListColumn::Height, "height"},
  {ThreeDZoneListColumn::SpeakerSetup, "speaker_setup"},
});

constexpr auto threeDZoneListColumnValues = std::array<ThreeDZoneListColumn, 4>{
  ThreeDZoneListColumn::Id,
  ThreeDZoneListColumn::Width,
  ThreeDZoneListColumn::Height,
  ThreeDZoneListColumn::SpeakerSetup,
};

constexpr ThreeDZoneListColumn operator|(ThreeDZoneListColumn lhs, ThreeDZoneListColumn rhs)
{
  return static_cast<ThreeDZoneListColumn>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

constexpr ThreeDZoneListColumn operator&(ThreeDZoneListColumn lhs, ThreeDZoneListColumn rhs)
{
  return static_cast<ThreeDZoneListColumn>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

constexpr ThreeDZoneListColumn operator~(ThreeDZoneListColumn value)
{
  return static_cast<ThreeDZoneListColumn>(~static_cast<uint8_t>(value));
}

constexpr bool contains(ThreeDZoneListColumn value, ThreeDZoneListColumn mask)
{
  return (static_cast<uint8_t>(value) & static_cast<uint8_t>(mask)) == static_cast<uint8_t>(mask);
}

#endif
