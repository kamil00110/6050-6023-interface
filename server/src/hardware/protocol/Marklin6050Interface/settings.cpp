#include "settings.hpp"
#include "../../../core/attributes.hpp"
#include "../../../utils/displayname.hpp"

namespace Marklin6050 {

Settings::Settings(Object& _parent, std::string_view parentPropertyName)
    : SubObject(_parent, parentPropertyName)
    , centralUnitVersion{this, "central_unit_version", 6020,
          PropertyFlags::ReadWrite | PropertyFlags::Store,
          [this](const uint16_t& value) { centralUnitVersionChanged(value); }}
    , analog{this, "analog", false, PropertyFlags::ReadWrite | PropertyFlags::Store}
    , s88amount{this, "s88amount", 1, PropertyFlags::ReadWrite | PropertyFlags::Store}
    , s88interval{this, "s88interval", 400, PropertyFlags::ReadWrite | PropertyFlags::Store}
    , turnouttime{this, "turnouttime", 200, PropertyFlags::ReadWrite | PropertyFlags::Store}
    , redundancy{this, "redundancy", 0, PropertyFlags::ReadWrite | PropertyFlags::Store}
    , extensions{this, "extensions", false, PropertyFlags::ReadWrite | PropertyFlags::Store}
    , oldAddress{this, "oldAddress", 1, PropertyFlags::ReadWrite | PropertyFlags::Store}
    , newAddress{this, "newAddress", 1, PropertyFlags::ReadWrite | PropertyFlags::Store}
    , programmer{this, "programmer", false, PropertyFlags::ReadWrite | PropertyFlags::Store}
{
    // --- Central Unit Version ---
    static const std::vector<unsigned int> cuOptions = {
        6020, 6021, 6022, 6023, 6223, 6027, 6029, 6030, 6032
    };
    static const std::vector<std::string_view> cuLabels = {
        "6020", "6021", "6022", "6023", "6223", "6027", "6029", "6030", "6032"
    };
    Attributes::addCategory(centralUnitVersion, "Märklin 6050");
    Attributes::addDisplayName(centralUnitVersion, "Central Unit Version");
    Attributes::addHelp(centralUnitVersion, "CUversion");
    Attributes::addEnabled(centralUnitVersion, true);
    Attributes::addVisible(centralUnitVersion, true);
    Attributes::addValues(centralUnitVersion, cuOptions);
    Attributes::addAliases(centralUnitVersion, &cuOptions, &cuLabels);
    m_interfaceItems.add(centralUnitVersion);

    // --- Analog mode ---
    Attributes::addCategory(analog, "Märklin 6050");
    Attributes::addDisplayName(analog, "Analog mode");
    Attributes::addEnabled(analog, false);
    Attributes::addVisible(analog, true);
    m_interfaceItems.add(analog);

    // --- S88 module amount ---
    Attributes::addCategory(s88amount, "Märklin 6050");
    Attributes::addDisplayName(s88amount, "s88 module amount");
    Attributes::addEnabled(s88amount, true);
    Attributes::addVisible(s88amount, true);
    Attributes::addMinMax(s88amount, 0u, 61u);
    m_interfaceItems.add(s88amount);

    // --- S88 interval ---
    static const std::vector<unsigned int> intervals = {
        50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1500, 2000, 2500, 3000
    };
    static const std::vector<std::string_view> intervalLabels = {
        "50ms", "100ms", "200ms", "300ms", "400ms", "500ms", "600ms",
        "700ms", "800ms", "900ms", "1s", "1.5s", "2s", "2.5s", "3s"
    };
    Attributes::addCategory(s88interval, "Märklin 6050");
    Attributes::addDisplayName(s88interval, "s88 call interval");
    Attributes::addEnabled(s88interval, true);
    Attributes::addVisible(s88interval, true);
    Attributes::addValues(s88interval, intervals);
    Attributes::addAliases(s88interval, &intervals, &intervalLabels);
    m_interfaceItems.add(s88interval);

    // --- Turnout time ---
    static const std::vector<unsigned int> turnoutTimes = {
        25, 50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000
    };
    static const std::vector<std::string_view> turnoutTimeLabels = {
        "25ms", "50ms", "100ms", "200ms", "300ms", "400ms",
        "500ms", "600ms", "700ms", "800ms", "900ms", "1s"
    };
    Attributes::addCategory(turnouttime, "Märklin 6050");
    Attributes::addDisplayName(turnouttime, "Accessory OFF time");
    Attributes::addEnabled(turnouttime, true);
    Attributes::addVisible(turnouttime, true);
    Attributes::addValues(turnouttime, turnoutTimes);
    Attributes::addAliases(turnouttime, &turnoutTimes, &turnoutTimeLabels);
    m_interfaceItems.add(turnouttime);

    // --- Redundancy ---
    static const std::vector<unsigned int> redundancyOptions = { 1, 2, 3, 4 };
    static const std::vector<std::string_view> redundancyLabels = { "OFF", "2x", "3x", "4x" };
    Attributes::addCategory(redundancy, "Märklin 6050");
    Attributes::addDisplayName(redundancy, "Command redundancy");
    Attributes::addEnabled(redundancy, true);
    Attributes::addVisible(redundancy, true);
    Attributes::addValues(redundancy, redundancyOptions);
    Attributes::addAliases(redundancy, &redundancyOptions, &redundancyLabels);
    m_interfaceItems.add(redundancy);

    // --- Extensions ---
    Attributes::addCategory(extensions, "Märklin 6050");
    Attributes::addDisplayName(extensions, "Feedback Module");
    Attributes::addEnabled(extensions, true);
    Attributes::addVisible(extensions, true);
    m_interfaceItems.add(extensions);

    // --- Programmer ---
    Attributes::addCategory(oldAddress, "Programmer");
    Attributes::addDisplayName(oldAddress, "Old loco address");
    Attributes::addEnabled(oldAddress, false);
    Attributes::addVisible(oldAddress, true);
    Attributes::addMinMax(oldAddress, 1u, 79u);
    m_interfaceItems.add(oldAddress);

    Attributes::addCategory(newAddress, "Programmer");
    Attributes::addDisplayName(newAddress, "New loco address");
    Attributes::addEnabled(newAddress, false);
    Attributes::addVisible(newAddress, true);
    Attributes::addMinMax(newAddress, 1u, 79u);
    m_interfaceItems.add(newAddress);

    Attributes::addCategory(programmer, "Programmer");
    Attributes::addDisplayName(programmer, "Change address");
    Attributes::addEnabled(programmer, false);
    Attributes::addVisible(programmer, true);
    m_interfaceItems.add(programmer);
}

Config Settings::config() const
{
    Config cfg;
    cfg.centralUnitVersion = centralUnitVersion;
    cfg.analog = analog;
    cfg.s88amount = s88amount;
    cfg.s88interval = s88interval;
    cfg.turnouttime = turnouttime;
    cfg.redundancy = redundancy;
    cfg.extensions = extensions;
    return cfg;
}

void Settings::loaded()
{
    SubObject::loaded();
    centralUnitVersionChanged(centralUnitVersion);
}

void Settings::updateEnabled(bool online)
{
    Attributes::setEnabled(centralUnitVersion, !online);
    Attributes::setEnabled(s88amount, !online);
    Attributes::setEnabled(s88interval, !online);
    Attributes::setEnabled(redundancy, !online);
    Attributes::setEnabled(extensions, !online);
    Attributes::setEnabled(oldAddress, online);
    Attributes::setEnabled(newAddress, online);
    Attributes::setEnabled(programmer, online);

    const uint16_t ver = centralUnitVersion;

    const bool analogSupport = (ver == 6027 || ver == 6029);
    Attributes::setEnabled(analog, analogSupport && !online);
    if (!analogSupport)
        analog.setValueInternal(false);

    if (ver == 6021)
        Attributes::setEnabled(turnouttime, false);
    else
        Attributes::setEnabled(turnouttime, !online);
}

void Settings::centralUnitVersionChanged(uint16_t /*value*/)
{
    updateEnabled(false);  // re-evaluate what's enabled
}

} // namespace Marklin6050
