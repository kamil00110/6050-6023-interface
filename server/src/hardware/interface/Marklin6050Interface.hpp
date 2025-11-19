#ifndef TRAINTASTIC_SERVER_HARDWARE_INTERFACE_MARKLIN6050INTERFACE_HPP
#define TRAINTASTIC_SERVER_HARDWARE_INTERFACE_MARKLIN6050INTERFACE_HPP

#include "interface.hpp"
#include "../../core/objectproperty.hpp"
#include "../../core/vectorproperty.hpp"
#include "../output/outputcontroller.hpp"
#include "../../hardware/protocol/Marklin6050Interface/serial.hpp"
#include "../../core/serialdeviceproperty.hpp"
#include "../../hardware/protocol/Marklin6050Interface/kernel.hpp"

class Marklin6050Interface 
: public Interface
, public OutputController
{
  CLASS_ID("interface.marklin6050")
  DEFAULT_ID("marklin6050")
  CREATE_DEF(Marklin6050Interface)

private:
  SerialDeviceProperty serialPort;
  Property<uint32_t> baudrate;
  Property<unsigned int> centralUnitVersion;
  Property<unsigned int> s88amount;
  Property<unsigned int> s88interval;
  Property<unsigned int> turnouttime;
  Property<unsigned int> extensions;
  Property<unsigned int> debug;
  Property<unsigned int> programmer;
  std::unique_ptr<Marklin6050::Kernel> m_kernel;
  void updateEnabled();
  void serialPortChanged(const std::string& newPort);

protected:
  void addToWorld() final;
  void loaded() final;
  void destroying() final;
  void worldEvent(WorldState state, WorldEvent event) final;
  void onlineChanged(bool value);  // remove 'final'

  bool setOnline(bool& value, bool simulation) final;

public:
  Marklin6050Interface(World& world, std::string_view id);
  // OutputController:
    std::span<const OutputChannel> outputChannels() const final;
    std::pair<uint32_t, uint32_t> outputAddressMinMax(OutputChannel channel) const final;
    [[nodiscard]] bool setOutputValue(OutputChannel channel, uint32_t address, OutputValue value) final;
};

#endif
