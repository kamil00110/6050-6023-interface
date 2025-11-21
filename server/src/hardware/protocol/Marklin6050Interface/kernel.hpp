#pragma once
#include <string>
#include <cstdint>
#include "../../output/outputvalue.hpp"   
namespace Marklin6050 {

class Kernel {
public:
    Kernel(const std::string& port, unsigned int baudrate = 2400);
    ~Kernel();

    int readByte();               
    bool sendByte(unsigned char byte);
    bool setAccessory(uint32_t address, OutputValue value, unsigned int timeMs);
    void setBaudRate(unsigned int baud) { m_baudrate = baud; }

private:
    std::string m_port;
    unsigned int m_baudrate;
    std::thread m_inputThread;          
    std::atomic<bool> m_running{false}; 

#if defined(_WIN32)
    void* m_handle;
#else
    int m_fd;
#endif
    bool m_isOpen;
};

} 
