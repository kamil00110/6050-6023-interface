#ifndef TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_3DZONELISTCOLUMN_HPP
#define TRAINTASTIC_SHARED_TRAINTASTIC_ENUM_3DZONELISTCOLUMN_HPP

#include <cstdint>
#include <array>

enum class ThreeDZoneListColumn : uint16_t
{
  Id = 1,
  Width = 2,
  Height = 4,
  SpeakerSetup = 8,
  Speaker0 = 16,
  Speaker1 = 32,
  Speaker2 = 64,
  Speaker3 = 128,
  Speaker4 = 256,
  Speaker5 = 512,
  Speaker6 = 1024,
  Speaker7 = 2048,
  Speaker8 = 4096,
  Speaker9 = 8192,
};

TRAINTASTIC_ENUM(ThreeDZoneListColumn, "3d_zone_list_column", 14,
{
  {ThreeDZoneListColumn::Id, "id"},
  {ThreeDZoneListColumn::Width, "width"},
  {ThreeDZoneListColumn::Height, "height"},
  {ThreeDZoneListColumn::SpeakerSetup, "speaker_setup"},
  {ThreeDZoneListColumn::Speaker0, "speaker_0"},
  {ThreeDZoneListColumn::Speaker1, "speaker_1"},
  {ThreeDZoneListColumn::Speaker2, "speaker_2"},
  {ThreeDZoneListColumn::Speaker3, "speaker_3"},
  {ThreeDZoneListColumn::Speaker4, "speaker_4"},
  {ThreeDZoneListColumn::Speaker5, "speaker_5"},
  {ThreeDZoneListColumn::Speaker6, "speaker_6"},
  {ThreeDZoneListColumn::Speaker7, "speaker_7"},
  {ThreeDZoneListColumn::Speaker8, "speaker_8"},
  {ThreeDZoneListColumn::Speaker9, "speaker_9"},
});

constexpr auto threeDZoneListColumnValues = std::array<ThreeDZoneListColumn, 14>{
  ThreeDZoneListColumn::Id,
  ThreeDZoneListColumn::Width,
  ThreeDZoneListColumn::Height,
  ThreeDZoneListColumn::SpeakerSetup,
  ThreeDZoneListColumn::Speaker0,
  ThreeDZoneListColumn::Speaker1,
  ThreeDZoneListColumn::Speaker2,
  ThreeDZoneListColumn::Speaker3,
  ThreeDZoneListColumn::Speaker4,
  ThreeDZoneListColumn::Speaker5,
  ThreeDZoneListColumn::Speaker6,
  ThreeDZoneListColumn::Speaker7,
  ThreeDZoneListColumn::Speaker8,
  ThreeDZoneListColumn::Speaker9,
};

constexpr ThreeDZoneListColumn operator|(ThreeDZoneListColumn lhs, ThreeDZoneListColumn rhs)
{
  return static_cast<ThreeDZoneListColumn>(static_cast<uint16_t>(lhs) | static_cast<uint16_t>(rhs));
}

constexpr ThreeDZoneListColumn operator&(ThreeDZoneListColumn lhs, ThreeDZoneListColumn rhs)
{
  return static_cast<ThreeDZoneListColumn>(static_cast<uint16_t>(lhs) & static_cast<uint16_t>(rhs));
}

constexpr ThreeDZoneListColumn operator~(ThreeDZoneListColumn value)
{
  return static_cast<ThreeDZoneListColumn>(~static_cast<uint16_t>(value));
}

constexpr bool contains(ThreeDZoneListColumn value, ThreeDZoneListColumn mask)
{
  return (static_cast<uint16_t>(value) & static_cast<uint16_t>(mask)) == static_cast<uint16_t>(mask);
}

#endif
