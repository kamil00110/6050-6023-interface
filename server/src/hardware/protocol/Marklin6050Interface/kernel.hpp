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
