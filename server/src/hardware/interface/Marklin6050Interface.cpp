#include "Marklin6050Interface.hpp"
#include "../output/list/outputlist.hpp"
#include "../input/list/inputlist.hpp"
#include "../../utils/displayname.hpp"  
#include "../../utils/makearray.hpp"
#include "../../world/world.hpp"
#include "../../core/serialdeviceproperty.hpp"
#include "../../hardware/protocol/Marklin6050Interface/serial_port_list.hpp"
#include "../../core/attributes.hpp"
#include "../../core/objectproperty.tpp"

constexpr auto inputListColumns = InputListColumn::Address;
constexpr auto outputListColumns = OutputListColumn::Channel | OutputListColumn::Address;


CREATE_IMPL(Marklin6050Interface)

Marklin6050Interface::Marklin6050Interface(World& world, std::string_view objId)
    : Interface(world, objId),
      OutputController(static_cast<IdObject&>(*this)),
      InputController(static_cast<IdObject&>(*this)),
      serialPort(this, "serialPort", "", PropertyFlags::ReadWrite | PropertyFlags::Store),
      baudrate(this, "baudrate", 2400, PropertyFlags::ReadWrite | PropertyFlags::Store), // default 2400
      centralUnitVersion(this, "centralUnitVersion", 0, PropertyFlags::ReadWrite | PropertyFlags::Store),
      s88amount(this, "s88amount", 0, PropertyFlags::ReadWrite | PropertyFlags::Store),
      s88interval(this, "s88interval", 0, PropertyFlags::ReadWrite | PropertyFlags::Store),
      turnouttime(this, "turnouttime", 0, PropertyFlags::ReadWrite | PropertyFlags::Store),
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
    50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000 
};
static const std::vector<std::string_view> turnouttimelabels = {
    "50ms", "100ms", "200ms", "300ms", "400ms", "500ms", "600ms", "700ms", "800ms", "900ms","1s",  
};
Attributes::addCategory(turnouttime, "Märklin 6050");
Attributes::addDisplayName(turnouttime, "s88 call intervall");
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

}

void Marklin6050Interface::addToWorld()
{
    Interface::addToWorld();
    InputController::addToWorld(inputListColumns);
    OutputController::addToWorld(outputListColumns);
}

void Marklin6050Interface::loaded()
{
    Interface::loaded();
    updateEnabled();
}

void Marklin6050Interface::destroying()
{
    Interface::destroying();
}

void Marklin6050Interface::worldEvent(WorldState state, WorldEvent event)
{
    Interface::worldEvent(state, event);

    if (!m_kernel)
        return;

    switch (event)
    {
        case WorldEvent::Stop:
            m_kernel->sendByte(97); // 0x61
            break;

        case WorldEvent::Run:
            m_kernel->sendByte(96); // 0x60
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

        setState(InterfaceState::Online);
    }
    else
    {
        if (m_kernel)
        {
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
    // Disable serialPort while online (same UX as LocoNet)
    Attributes::setEnabled(serialPort, !online);
}

void Marklin6050Interface::serialPortChanged(const std::string& newPort)
{
    // Use the interface's 'online' flag instead of accessing ObjectProperty<InterfaceStatus>
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
        // Only allow addresses in valid range
        auto [min, max] = outputAddressMinMax(channel);
        if(address < min || address > max)
            return false;

        // send to kernel
        bool result = m_kernel->setAccessory(address, value);
        if(result)
            updateOutputValue(channel, address, value);

        return result;
    }

    // fallback for unsupported channels
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
    static const auto values = makeArray(InputChannel::S88); // only valid channels
    return values;
}

std::pair<uint32_t, uint32_t> Marklin6050Interface::inputAddressMinMax(InputChannel channel) const
{
    switch(channel)
    {
        case InputChannel::S88:
            return {1, 61}; // S88 module range
        default:
            return {0, 0}; // unknown channel
    }
}


void Marklin6050Interface::inputSimulateChange(InputChannel channel, uint32_t address, SimulateInputAction action)
{
    (void)channel;
    (void)address;
    (void)action;

    // If you want to simulate input changes later, add logic here
}









