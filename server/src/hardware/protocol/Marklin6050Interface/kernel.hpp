#pragma once
#include <string>
#include <cstdint>
#include "../../output/outputvalue.hpp"   // defines OutputValue
// DO NOT include Marklin6050Interface.hpp here (avoids circular include)

namespace Marklin6050 {

class Kernel {
public:
    Kernel(const std::string& port, unsigned int baudrate = 2400);
    ~Kernel();

    bool start();
    void stop();

    bool sendByte(unsigned char byte);

    // handle accessory outputs (address 1..32)
    bool setAccessory(uint32_t address, OutputValue value, unsigned int timeMs);

    void setBaudRate(unsigned int baud) { m_baudrate = baud; }

private:
    std::string m_port;
    unsigned int m_baudrate;

#if defined(_WIN32)
    void* m_handle;
#else
    int m_fd;
#endif
    bool m_isOpen;
};

} // namespace Marklin6050
