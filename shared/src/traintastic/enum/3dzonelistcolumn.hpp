#ifndef TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_3DZONELISTCOLUMN_HPP
#define TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_3DZONELISTCOLUMN_HPP

#include <cstdint>
#include <array>
#include <type_traits>

enum class ThreeDZoneListColumn
{
  Id = 1 << 0,
  Width = 1 << 1,
  Height = 1 << 2,
  SpeakerSetup = 1 << 3,
  Speakers = 1 << 4,
};

constexpr std::array<ThreeDZoneListColumn, 5> threeDZoneListColumnValues = {
  ThreeDZoneListColumn::Id,
  ThreeDZoneListColumn::Width,
  ThreeDZoneListColumn::Height,
  ThreeDZoneListColumn::SpeakerSetup,
  ThreeDZoneListColumn::Speakers,
};

constexpr ThreeDZoneListColumn operator|(const ThreeDZoneListColumn lhs, const ThreeDZoneListColumn rhs)
{
  return static_cast<ThreeDZoneListColumn>(static_cast<std::underlying_type_t<ThreeDZoneListColumn>>(lhs) | static_cast<std::underlying_type_t<ThreeDZoneListColumn>>(rhs));
}

constexpr bool contains(const ThreeDZoneListColumn value, const ThreeDZoneListColumn mask)
{
  return (static_cast<std::underlying_type_t<ThreeDZoneListColumn>>(value) & static_cast<std::underlying_type_t<ThreeDZoneListColumn>>(mask)) == static_cast<std::underlying_type_t<ThreeDZoneListColumn>>(mask);
}

#endif
