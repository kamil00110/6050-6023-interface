#include kernel.hpp
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN   // Exclude rarely-used stuff from Windows headers
#include <winsock2.h>         // Must be before windows.h
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#endif

#include <thread>
#include <atomic>
#include <chrono>

using namespace Marklin6050;

Kernel::Kernel(const std::string& port, unsigned int baudrate)
    : m_port(port), m_baudrate(baudrate), m_isOpen(false)
{
#if defined(_WIN32)
    m_handle = INVALID_HANDLE_VALUE;
#else
    m_fd = -1;
#endif
}

Kernel::~Kernel()
{
    stop();
}

bool Kernel::start()
{
#if defined(_WIN32)
    std::string devicePath = "\\\\.\\" + m_port;
    m_handle = CreateFileA(
        devicePath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (m_handle == INVALID_HANDLE_VALUE)
        return false;

    DCB dcb{};
    if (!GetCommState(m_handle, &dcb))
        return false;

    dcb.BaudRate = m_baudrate;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    if (!SetCommState(m_handle, &dcb))
        return false;

    m_isOpen = true;
    return true;

#else
    m_fd = open(m_port.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (m_fd < 0)
        return false;

    termios options{};
    tcgetattr(m_fd, &options);

    speed_t speed;
    switch (m_baudrate) {
        case 1200:   speed = B1200; break;
        case 2400:   speed = B2400; break;
        case 4800:   speed = B4800; break;
        case 9600:   speed = B9600; break;
        case 19200:  speed = B19200; break;
        case 38400:  speed = B38400; break;
        case 57600:  speed = B57600; break;
        case 115200: speed = B115200; break;
        default:     speed = B2400; break; // fallback
    }

    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;

    tcsetattr(m_fd, TCSANOW, &options);

    m_isOpen = true;
    return true;
#endif
}

void Kernel::stop()
{


    if (!m_isOpen)
        return;

#if defined(_WIN32)
    if (m_handle != INVALID_HANDLE_VALUE)
        CloseHandle(m_handle);
    m_handle = INVALID_HANDLE_VALUE;
#else
    if (m_fd >= 0)
        close(m_fd);
    m_fd = -1;
#endif

    m_isOpen = false;
}
#include <thread>
#include <chrono>

bool Kernel::setAccessory(uint32_t address, OutputValue value, unsigned int timeMs)
{
    if (!m_isOpen || address < 1 || address > 256)
        return false;

    unsigned char cmd = 0;

    std::visit([&](auto&& v){
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, OutputPairValue>) {
            cmd = (v == OutputPairValue::First) ? 34 : 33;
        }
        else if constexpr (std::is_same_v<T, TriState>) {
            cmd = (v == TriState::True) ? 34 : 33;
        }
        else {
            cmd = static_cast<unsigned char>(v);
        }
    }, value);

    if (!sendByte(cmd) || !sendByte(static_cast<unsigned char>(address)))
        return false;

    if (timeMs > 0) {
        std::thread([this, address, timeMs]{
            std::this_thread::sleep_for(std::chrono::milliseconds(timeMs));
            sendByte(32);
            sendByte(static_cast<unsigned char>(address));
        }).detach();
    }

    return true;
}

bool Kernel::sendByte(unsigned char byte)
{
    if (!m_isOpen)
        return false;

#if defined(_WIN32)
    DWORD written = 0;
    WriteFile(m_handle, &byte, 1, &written, nullptr);
    return written == 1;
#else
    return write(m_fd, &byte, 1) == 1;
#endif
}
int Kernel::readByte()
{
#if defined(_WIN32)
    unsigned char b;
    DWORD read = 0;
    if (ReadFile(m_handle, &b, 1, &read, nullptr) && read == 1)
        return b;
    return -1;
#else
    unsigned char b;
    int r = ::read(m_fd, &b, 1);
    return (r == 1) ? b : -1;
#endif
}
void Kernel::startInputThread(unsigned int moduleCount, unsigned int intervalMs)
{
    if(m_running)
        return;

    m_running = true;
    m_inputThread = std::thread([this, moduleCount, intervalMs]() {
        while (m_running) {
            // Poll S88 inputs
            inputLoop(moduleCount, intervalMs);
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    });
}

void Kernel::stopInputThread()
{
    if(!m_running)
        return;

    m_running = false;
    if(m_inputThread.joinable())
        m_inputThread.join();
}


void Kernel::inputLoop(unsigned int modules, unsigned int intervalMs)
{
    const unsigned char cmd = 128 + modules; // S88 command

    while (m_running)
    {
        sendByte(cmd);

        const unsigned int totalBytes = modules * 2;

        std::vector<uint8_t> buffer(totalBytes);

        for (unsigned int i = 0; i < totalBytes; i++)
        {
            int b = readByte();
            if (b < 0)
                break;

            buffer[i] = (uint8_t)b;
        }

        // Convert to bit states (1.. modules*16)
        for (unsigned int m = 0; m < modules; m++)
        {
            uint8_t high = buffer[m * 2];
            uint8_t low  = buffer[m * 2 + 1];

            uint16_t bits = (high << 8) | low;

            for (int bit = 0; bit < 16; bit++)
            {
                bool state = bits & (1 << bit);
                uint32_t address = m * 16 + (bit + 1);
                (void)state;
                (void)address;

            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
}

