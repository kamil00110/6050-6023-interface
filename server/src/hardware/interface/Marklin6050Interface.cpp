#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#endif

#include "Marklin6050Interface.hpp"
#include "../output/list/outputlist.hpp"
#include "../input/list/inputlist.hpp"
#include "../decoder/list/decoderlist.hpp"
#include "../decoder/list/decoderlisttablemodel.hpp"
#include "../../utils/displayname.hpp"  
#include "../../utils/makearray.hpp"
#include "../../world/world.hpp"
#include "../../core/serialdeviceproperty.hpp"
#include "../../hardware/protocol/Marklin6050Interface/serial_port_list.hpp"
#include "../../core/attributes.hpp"
#include "../../core/objectproperty.tpp"
#include "../../core/eventloop.hpp"
#include "../../hardware/protocol/Marklin6050Interface/kernel.hpp"
#include "../../log/log.hpp"
#include "../../log/logmessageexception.hpp"
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>


constexpr auto inputListColumns = InputListColumn::Address;
constexpr auto outputListColumns = OutputListColumn::Channel | OutputListColumn::Address;
constexpr auto decoderListColumns = DecoderListColumn::Id | DecoderListColumn::Name | DecoderListColumn::Address;


CREATE_IMPL(Marklin6050Interface)

Marklin6050Interface::Marklin6050Interface(World& world, std::string_view objId)
    : Interface(world, objId),
      OutputController(static_cast<IdObject&>(*this)),
      InputController(static_cast<IdObject&>(*this)),
      DecoderController(*this, decoderListColumns),
      serialPort(this, "serialPort", "", PropertyFlags::ReadWrite | PropertyFlags::Store),
      baudrate(this, "baudrate", 2400, PropertyFlags::ReadWrite | PropertyFlags::Store),
      centralUnitVersion(this, "centralUnitVersion", 6020, PropertyFlags::ReadWrite | PropertyFlags::Store),
      s88amount(this, "s88amount", 1, PropertyFlags::ReadWrite | PropertyFlags::Store),
      s88interval(this, "s88interval", 400, PropertyFlags::ReadWrite | PropertyFlags::Store),
      turnouttime(this, "turnouttime", 200, PropertyFlags::ReadWrite | PropertyFlags::Store),
      slowacceleration(this, "slowacceleration", 0, PropertyFlags::ReadWrite | PropertyFlags::Store),
      slowdeceleration(this, "slowdeceleration", 0, PropertyFlags::ReadWrite | PropertyFlags::Store),
      redundancy(this, "redundancy", 0, PropertyFlags::ReadWrite | PropertyFlags::Store),
      extensions(this, "extensions", false, PropertyFlags::ReadWrite | PropertyFlags::Store),
      debug(this, "debug", false, PropertyFlags::ReadWrite | PropertyFlags::Store),
      oldAddress(this, "oldAddress", 1, PropertyFlags::ReadWrite | PropertyFlags::Store),
      newAddress(this, "newAddress", 1, PropertyFlags::ReadWrite | PropertyFlags::Store),
      programmer(this, "programmer", false, PropertyFlags::ReadWrite | PropertyFlags::Store)
{    
    name = "Märklin 6050";

    Attributes::addDisplayName(serialPort, DisplayName::Serial::device);
    Attributes::addEnabled(serialPort, !online);
    Attributes::addVisible(serialPort, true);
    m_interfaceItems.insertBefore(serialPort, notes);

    Attributes::addDisplayName(baudrate, DisplayName::Serial::baudrate);
    Attributes::addEnabled(baudrate, !online);
    Attributes::addVisible(baudrate, true);
    m_interfaceItems.insertBefore(baudrate, notes);
    Attributes::addValues(baudrate, std::vector<unsigned int>{
    1200,
    2400,
    4800,
    9600,
    19200,
    38400,
    57600,
    115200
    });
    
    static const std::vector<unsigned int> options = {
    6020, 6021, 6022, 6023,
    6223,
    6027, 6029, 6030, 6032
};
static const std::vector<std::string_view> labels = {
    "6020", "6021", "6022", "6023",
    "6223",
    "6027", "6029", "6030", "6032"
};
Attributes::addCategory(centralUnitVersion, "Märklin 6050");
Attributes::addDisplayName(centralUnitVersion, "Central Unit Version");
Attributes::addHelp(centralUnitVersion, "CUversion");
Attributes::addEnabled(centralUnitVersion, true);
Attributes::addVisible(centralUnitVersion, true);
m_interfaceItems.insertBefore(centralUnitVersion, notes);
Attributes::addValues(centralUnitVersion, options);
Attributes::addAliases(centralUnitVersion, &options, &labels);

Attributes::addCategory(s88amount, "Märklin 6050");
Attributes::addDisplayName(s88amount, "s88 module amount");
Attributes::addHelp(s88amount, "CU.s88amount");
Attributes::addEnabled(s88amount, !online);
Attributes::addVisible(s88amount, true);
m_interfaceItems.insertBefore(s88amount, notes);
Attributes::addMinMax(s88amount, 0u, 61u); 

static const std::vector<unsigned int> intervals = {
    50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000 
};
static const std::vector<std::string_view> intervallabels = {
    "50ms", "100ms", "200ms", "300ms", "400ms", "500ms", "600ms", "700ms", "800ms", "900ms","1s",  
};
Attributes::addCategory(s88interval, "Märklin 6050");
Attributes::addDisplayName(s88interval, "s88 call intervall");
Attributes::addHelp(s88interval, "CU.s88intervall");
Attributes::addEnabled(s88interval, !online);
Attributes::addVisible(s88interval, true);
m_interfaceItems.insertBefore(s88interval, notes);
Attributes::addValues(s88interval, intervals);
Attributes::addAliases(s88interval, &intervals, &intervallabels);

static const std::vector<unsigned int> turnouttimes = {
    25, 50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000 
};
static const std::vector<std::string_view> turnouttimelabels = {
    "25ms", "50ms", "100ms", "200ms", "300ms", "400ms", "500ms", "600ms", "700ms", "800ms", "900ms","1s",  
};
Attributes::addCategory(turnouttime, "Märklin 6050");
Attributes::addDisplayName(turnouttime, "Accessory OFF time");
Attributes::addHelp(turnouttime, "CU.s88intervall");
Attributes::addEnabled(turnouttime, !online);
Attributes::addVisible(turnouttime, true);
m_interfaceItems.insertBefore(turnouttime, notes);
Attributes::addValues(turnouttime, turnouttimes);
Attributes::addAliases(turnouttime, &turnouttimes, &turnouttimelabels);

static const std::vector<unsigned int> slowacceltimes = {
    0,1000,2000,3000,4000,5000
};
static const std::vector<std::string_view> slowaccellabels = {
    "OFF/Auto", "1s", "2s", "3s", "4s", "5s",  
};
    
Attributes::addCategory(slowacceleration, "Märklin 6050");
Attributes::addDisplayName(slowacceleration, "Acceleration time");
Attributes::addHelp(slowacceleration, "CU.s88intervall");
Attributes::addEnabled(slowacceleration, !online);
Attributes::addVisible(slowacceleration, true);
m_interfaceItems.insertBefore(slowacceleration, notes);
Attributes::addValues(slowacceleration, slowacceltimes);
Attributes::addAliases(slowacceleration, &slowacceltimes, &slowaccellabels);

static const std::vector<unsigned int> slowdeceltimes = {
    0,1000,2000,3000,4000,5000
};
static const std::vector<std::string_view> slowdecellabels = {
    "OFF/Auto", "1s", "2s", "3s", "4s", "5s",  
};
    
Attributes::addCategory(slowdeceleration, "Märklin 6050");
Attributes::addDisplayName(slowdeceleration, "Deceleration time");
Attributes::addHelp(slowdeceleration, "CU.s88intervall");
Attributes::addEnabled(slowdeceleration, !online);
Attributes::addVisible(slowdeceleration, true);
m_interfaceItems.insertBefore(slowdeceleration, notes);
Attributes::addValues(slowdeceleration, slowdeceltimes);
Attributes::addAliases(slowdeceleration, &slowdeceltimes, &slowdecellabels);

static const std::vector<unsigned int> redundancyamount = {
    1,2,3,4
};
static const std::vector<std::string_view> redundancylabels = {
    "OFF", "2x", "3x", "4x",
};
    
Attributes::addCategory(redundancy, "Märklin 6050");
Attributes::addDisplayName(redundancy, "Command redundancy");
Attributes::addHelp(redundancy, "CU.s88intervall");
Attributes::addEnabled(redundancy, !online);
Attributes::addVisible(redundancy, true);
m_interfaceItems.insertBefore(redundancy, notes);
Attributes::addValues(redundancy, redundancyamount);
Attributes::addAliases(redundancy, &redundancyamount, &redundancylabels);

Attributes::addCategory(extensions, "Märklin 6050");
Attributes::addDisplayName(extensions, "Feedback Module");
Attributes::addEnabled(extensions, !online);
Attributes::addVisible(extensions, true);
m_interfaceItems.insertBefore(extensions, notes);

Attributes::addCategory(debug, "Märklin 6050");
Attributes::addDisplayName(debug, "Seiral Activity");
Attributes::addEnabled(debug, !online);
Attributes::addVisible(debug, true);
m_interfaceItems.insertBefore(debug, notes);

Attributes::addCategory(oldAddress, "Programmer");
Attributes::addDisplayName(oldAddress, "Old loco address");
Attributes::addEnabled(oldAddress, !online);
Attributes::addVisible(oldAddress, true);
m_interfaceItems.insertBefore(oldAddress, notes);
Attributes::addMinMax(oldAddress, 1u, 79u);

Attributes::addCategory(newAddress, "Programmer");
Attributes::addDisplayName(newAddress, "New loco address");
Attributes::addEnabled(newAddress, !online);
Attributes::addVisible(newAddress, true);
m_interfaceItems.insertBefore(newAddress, notes);
Attributes::addMinMax(newAddress, 1u, 79u);
    
Attributes::addCategory(programmer, "Programmer");
Attributes::addDisplayName(programmer, "Change address");
Attributes::addEnabled(programmer, !online);
Attributes::addVisible(programmer, true);
m_interfaceItems.insertBefore(programmer, notes);

m_interfaceItems.insertBefore(inputs, notes);
    
m_interfaceItems.insertBefore(outputs, notes);

m_interfaceItems.insertBefore(decoders, notes);

}

void Marklin6050Interface::addToWorld()
{
    Interface::addToWorld();
    InputController::addToWorld(inputListColumns);
    OutputController::addToWorld(outputListColumns);
    DecoderController::addToWorld();
}

void Marklin6050Interface::loaded()
{
    Interface::loaded();
    updateEnabled();
}

void Marklin6050Interface::destroying()
{
    Interface::destroying();
    OutputController::destroying();
    InputController::destroying();
    DecoderController::destroying(); 
}

void Marklin6050Interface::worldEvent(WorldState state, WorldEvent event)
{
    Interface::worldEvent(state, event);

    if (!m_kernel)
        return;

    switch (event)
    {
        case WorldEvent::Stop:
            m_kernel->sendByte(130);
            break;

        case WorldEvent::Run:
            m_kernel->sendByte(96);
            break;

        default:
            break;
    }
}


void Marklin6050Interface::onlineChanged(bool /*value*/)
{
    updateEnabled();
}

bool Marklin6050Interface::setOnline(bool& value, bool /*simulation*/)
{
    std::string port = serialPort;
    
    if (value)
    {
        
        if (port.empty() || !Marklin6050::Serial::isValidPort(port))
        {
            value = false;
            return false;
        }
     
        if (!Marklin6050::Serial::testOpen(port))
        {
            value = false;
            return false;
        }

        
        m_kernel = std::make_unique<Marklin6050::Kernel>(port, baudrate.value());
        m_kernel->s88Callback = [this](uint32_t address, bool state)
{

        this->onS88Input(address, state);
};
       if (!m_kernel->start())
       {
            m_kernel.reset();
            value = false;
            return false;
       }
        m_kernel->startInputThread(s88amount.value(), s88interval.value());
        setState(InterfaceState::Online);
    }
    else
    {
        if (m_kernel)
        {
            m_kernel->stopInputThread();
            m_kernel->stop();
            m_kernel.reset();
        }
        setState(InterfaceState::Offline);
    }

    updateEnabled();
    return true;
}

void Marklin6050Interface::updateEnabled()
{
    Attributes::setEnabled(serialPort, !online);
}

void Marklin6050Interface::serialPortChanged(const std::string& newPort)
{
    if (online)
    {
        if (!Marklin6050::Serial::isValidPort(newPort) || !Marklin6050::Serial::testOpen(newPort))
        {
            bool val = false;
            setOnline(val, false);
        }
    }
}
bool Marklin6050Interface::setOutputValue(OutputChannel channel, uint32_t address, OutputValue value)
{
    if(channel == OutputChannel::Accessory && m_kernel)
    {
        auto [min, max] = outputAddressMinMax(channel);
        if(address < min || address > max)
            return false;

        unsigned int delayMs = turnouttime.value();

        bool result = m_kernel->setAccessory(address, value, delayMs);

        if(result)
            updateOutputValue(channel, address, value);

        return result;
    }
    if(channel == OutputChannel::Turnout && m_kernel)
    {
        auto [min, max] = outputAddressMinMax(channel);
        if(address < min || address > max)
            return false;

        unsigned int delayMs = turnouttime.value(); 

        bool result = m_kernel->setAccessory(address, value, delayMs);

        if(result)
            updateOutputValue(channel, address, value);

        return result;
    }
    if(channel == OutputChannel::Output && m_kernel)
    {
        auto [min, max] = outputAddressMinMax(channel);
        if(address < min || address > max)
            return false;

        unsigned int delayMs = turnouttime.value(); 

        bool result = m_kernel->setAccessory(address, value, delayMs);

        if(result)
            updateOutputValue(channel, address, value);

        return result;
    }

    return false;
}


std::pair<uint32_t, uint32_t> Marklin6050Interface::outputAddressMinMax(OutputChannel channel) const
{
    switch(channel)
    {
        case OutputChannel::Accessory:
            return {1, 256};
        case OutputChannel::Turnout:
        case OutputChannel::Output:
            return {1, 256}; 
        default:
            return OutputController::outputAddressMinMax(channel);
    }
}


std::span<const OutputChannel> Marklin6050Interface::outputChannels() const
{
    static const auto values = makeArray(
        OutputChannel::Accessory,
        OutputChannel::Turnout,
        OutputChannel::Output
    );
    return values;
}
std::span<const InputChannel> Marklin6050Interface::inputChannels() const
{
    static const auto values = makeArray(InputChannel::S88);
    return values;
}

std::pair<uint32_t, uint32_t> Marklin6050Interface::inputAddressMinMax(InputChannel channel) const
{
    switch(channel)
    {
        case InputChannel::S88:
        {
            uint32_t moduleCount = s88amount.value();   
            uint32_t maxAddress = moduleCount * 16;     
            return {1, maxAddress};        
        }
        default:
            return {0, 0}; 
    }
}
void Marklin6050Interface::inputSimulateChange(InputChannel channel, uint32_t address, SimulateInputAction action)
{
    (void)channel;
    (void)address;
    (void)action;

}

void Marklin6050Interface::onS88Input(uint32_t address, bool state)
{
    std::string info = "S88 address " + std::to_string(address) + " -> " + (state ? "ON" : "OFF");

#if defined(_WIN32)
 

    // Get Documents folder
    PWSTR docPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &docPath)))
    {
        char utf8Path[MAX_PATH * 4];
        int len = WideCharToMultiByte(CP_UTF8, 0, docPath, -1, utf8Path, sizeof(utf8Path), NULL, NULL);
        CoTaskMemFree(docPath);

        if (len > 0)
        {
            std::string fullPath = std::string(utf8Path) + "\\S88_log.txt";
            FILE* f = fopen(fullPath.c_str(), "a");
            if (f)
            {
                fprintf(f, "%s\n", info.c_str());
                fclose(f);
            }
            else
            {
                MessageBoxA(nullptr, "Failed to write S88_log.txt in Documents", "Logging Error", MB_OK | MB_ICONERROR);
            }
        }
    }
#else
    // Non-Windows: original logging code could go here (optional)
#endif

    // Update InputController state
    TriState ts = state ? TriState::True : TriState::False;
    updateInputValue(InputChannel::S88, address, ts);
}




std::span<const DecoderProtocol> Marklin6050Interface::decoderProtocols() const
{
 
    static constexpr std::array<DecoderProtocol, 1> protocols{
        DecoderProtocol::None
    };

    return std::span<const DecoderProtocol>{protocols.data(), protocols.size()};
}

std::pair<uint16_t, uint16_t>
Marklin6050Interface::decoderAddressMinMax(DecoderProtocol /*protocol*/) const
{
    return {1, 255};
}

std::span<const uint8_t>
Marklin6050Interface::decoderSpeedSteps(DecoderProtocol /*protocol*/) const
{
    static constexpr uint8_t steps[] = { 14 };
    return std::span<const uint8_t>(steps, 1);
}

void Marklin6050Interface::decoderChanged(
    const Decoder& /*decoder*/,
    DecoderChangeFlags /*changes*/,
    uint32_t /*functionNumber*/)
{
  
}






