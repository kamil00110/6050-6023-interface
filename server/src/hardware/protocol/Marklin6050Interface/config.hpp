#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_CONFIG_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MARKLIN6050_CONFIG_HPP

#include <cstdint>

namespace Marklin6050 {

struct Config
{
    uint16_t centralUnitVersion = 6020;
    bool analog = false;
    unsigned int s88amount = 0;
    unsigned int s88interval = 400;
    unsigned int turnouttime = 200;
    unsigned int redundancy = 0;
    bool extensions = false;
};

} // namespace Marklin6050

#endif
