#include "3dSound.hpp"
#include "../../core/attributes.hpp"

3dSound::3dSound(World& world, std::string_view _id)
    : IdObject(world, _id)
    , name{this, "name", std::string{}, PropertyFlags::ReadWrite | PropertyFlags::Store}
    , x{this, "x", 0.0f, PropertyFlags::ReadWrite | PropertyFlags::Store}
    , y{this, "y", 0.0f, PropertyFlags::ReadWrite | PropertyFlags::Store}
    , z{this, "z", 0.0f, PropertyFlags::ReadWrite | PropertyFlags::Store}
    , volume{this, "volume", 1.0f, PropertyFlags::ReadWrite | PropertyFlags::Store}
    , pitch{this, "pitch", 1.0f, PropertyFlags::ReadWrite | PropertyFlags::Store}
{
    Attributes::addDisplayName(name, "3D Sound Name");
    Attributes::addDisplayName(x, "Position X");
    Attributes::addDisplayName(y, "Position Y");
    Attributes::addDisplayName(z, "Position Z");
    Attributes::addDisplayName(volume, "Volume");
    Attributes::addDisplayName(pitch, "Pitch");
}

void 3dSound::addToWorld()
{
    IdObject::addToWorld();
    // Here you could add to a 3D sound manager if you have one
}

void 3dSound::loaded()
{
    IdObject::loaded();
}

void 3dSound::destroying()
{
    IdObject::destroying();
}

void 3dSound::play()
{
    onPlay.fire();
    // Could trigger your actual 3D sound playback here
}

