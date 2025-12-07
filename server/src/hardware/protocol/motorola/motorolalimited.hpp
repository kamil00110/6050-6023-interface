/**
 * server/src/hardware/protocol/motorola/motorola_limited.hpp
 *
 * Motorola Limited protocol
 *
 * Supports only addresses 10, 20, 30, 40.
 */

#ifndef TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MOTOROLA_MOTOROLA_LIMITED_HPP
#define TRAINTASTIC_SERVER_HARDWARE_PROTOCOL_MOTOROLA_MOTOROLA_LIMITED_HPP

#include <cstdint>
#include <array>

namespace MotorolaLimited
{
    // Allowed locomotive addresses
    static constexpr std::array<uint16_t, 4> validAddresses{
        10, 20, 30, 40
    };

    // Optional: min/max for UI display
    static constexpr uint16_t addressMin = 10;
    static constexpr uint16_t addressMax = 40;

    // Utility: check if address is valid
    inline bool isValidAddress(uint16_t address)
    {
        for (auto a : validAddresses)
            if (a == address)
                return true;
        return false;
    }
}

#endif
