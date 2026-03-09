#pragma once
#include <string>
#include <cstdint>
#include <thread>
#include <functional>
#include <atomic>

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
    bool start();
    void stop();
    bool isRunning() const { return m_running.load(); }
    void startInputThread(unsigned int moduleCount, unsigned int intervalMs);
    void stopInputThread();
    std::function<void(uint32_t, bool)> s88Callback;
    void setLocoSpeed(uint8_t address, uint8_t speed, bool f0);
    void setLocoDirection(uint8_t address, bool f0);
    void setLocoFunction(uint8_t address, uint8_t currentSpeed, bool f0);
    void setRedundancy(unsigned int count);
    void setLocoFunctions1to4(uint8_t address, bool f1, bool f2, bool f3, bool f4);
    void setLocoEmergencyStop(uint8_t address, bool f0);

private:
    std::string m_port;
    unsigned int m_baudrate;
    std::thread m_inputThread;          
    std::atomic<bool> m_running{false}; 
    void inputLoop(unsigned int modules);
    unsigned int m_redundancy{0};
    void sendCommand(uint8_t byte1, uint8_t byte2);


#if defined(_WIN32)
    void* m_handle;
#else
    int m_fd;
#endif
    bool m_isOpen;
};

} 

namespace Command
{
    constexpr uint8_t LocoStop        = 0;    // speed 0
    constexpr uint8_t LocoSpeedMin    = 1;    // speed step 1
    constexpr uint8_t LocoSpeedMax    = 14;   // speed step 14
    constexpr uint8_t LocoDirToggle   = 15;   // direction toggle
    constexpr uint8_t LocoF0Bit       = 0x10; // bit 4 = F0 on

    constexpr uint8_t AccessoryOff    = 32;
    constexpr uint8_t AccessoryGreen  = 33;
    constexpr uint8_t AccessoryRed    = 34;

    constexpr uint8_t FunctionBase    = 64;   // F1-F4 bitmask added to this

    constexpr uint8_t GlobalGo        = 96;
    constexpr uint8_t GlobalStop      = 97;

    constexpr uint8_t S88Base         = 128;  // + module count
}
