#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
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
      baudrate(this, "baudrate", 2400, PropertyFlags::ReadWrite | PropertyFlags::Store), // default 2400
      centralUnitVersion(this, "centralUnitVersion", 6020, PropertyFlags::ReadWrite | PropertyFlags::Store),
      s88amount(this, "s88amount", 1, PropertyFlags::ReadWrite | PropertyFlags::Store),
      s88interval(this, "s88interval", 400, PropertyFlags::ReadWrite | PropertyFlags::Store),
      turnouttime(this, "turnouttime", 200, PropertyFlags::ReadWrite | PropertyFlags::Store),
      extensions(this, "extensions", false, PropertyFlags::ReadWrite | PropertyFlags::Store),
      debug(this, "debug", 0, PropertyFlags::ReadWrite | PropertyFlags::Store),
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

Attributes::addCategory(programmer, "Programmer");
Attributes::addDisplayName(programmer, "Programer");
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
            m_kernel->sendByte(97);
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
       if (!m_kernel->start())
       {
            m_kernel.reset();
            value = false;
            return false;
       }
       if (!m_kernel->start()) {
            m_kernel.reset();
            value = false;
           return false;
       }

        setState(InterfaceState::Online);
    }
    else
    {
        if (m_kernel)
        {
            m_kernel->stop();
            m_kernel.reset();
        }
        EventLoop::deleteLater(m_kernel.release());
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
    // Convert bool to TriState expected by InputController
    TriState ts = state ? TriState::True : TriState::False;

    // Update the input value so the engine/world receives the change
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

void Marklin6050Interface::s88Loop()
{
    while (m_runS88)
    {
        readS88();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(s88interval.value())
        );
    }
}

void Marklin6050Interface::readS88()
{
    if (!m_kernel || !m_kernel->isRunning())
        return;

    uint32_t moduleCount = s88amount.value();
    uint32_t totalBits   = moduleCount * 16;

    if (moduleCount == 0)
        return;

 
    unsigned char cmd = 128 + moduleCount;
    if (!m_kernel->sendByte(cmd))
        return;

    std::vector<bool> newState(totalBits);

    for (uint32_t i = 0; i < totalBits; i++)
    {
        int val = m_kernel->readByte();
        if (val < 0)
            return;

        newState[i] = (val != 0);
    }

    for (uint32_t i = 0; i < totalBits; i++)
    {
        if (newState[i] != m_lastS88State[i])
        {
            uint32_t address = i + 1; 
            TriState ts = newState[i] ? TriState::True : TriState::False;

            updateInputValue(InputChannel::S88, address, ts);
        }
    }

    m_lastS88State = newState;
}






