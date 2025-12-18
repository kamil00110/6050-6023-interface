#include "3dSound.hpp"
#include "list/3dSoundList.hpp"
#include "../../world/world.hpp"
#include "../../core/attributes.hpp"

std::shared_ptr<Sound3D> Sound3D::create(World& world, std::string_view _id)
{
    auto sound = std::make_shared<Sound3D>(world, _id);
    sound->addToWorld();
    return sound;
}

Sound3D::Sound3D(World& world, std::string_view _id)
    : Object(world, _id)
    , name(this, "name", id, PropertyFlags::ReadWrite | PropertyFlags::Store)
    , x(this, "x", 0.0f, PropertyFlags::ReadWrite | PropertyFlags::Store)
    , y(this, "y", 0.0f, PropertyFlags::ReadWrite | PropertyFlags::Store)
    , z(this, "z", 0.0f, PropertyFlags::ReadWrite | PropertyFlags::Store)
    , volume(this, "volume", 1.0f, PropertyFlags::ReadWrite | PropertyFlags::Store)
    , pitch(this, "pitch", 1.0f, PropertyFlags::ReadWrite | PropertyFlags::Store)
    , looping(this, "looping", false, PropertyFlags::ReadWrite | PropertyFlags::Store)
    , play(*this, "play", MethodFlags::ScriptCallable, [this]() { fireEvent("play"); })
    , stop(*this, "stop", MethodFlags::ScriptCallable, [this]() { fireEvent("stop"); })
    , pause(*this, "pause", MethodFlags::ScriptCallable, [this]() { fireEvent("pause"); })
    , onEvent(*this, "on_event", EventFlags::Scriptable)
{
    m_interfaceItems.add(name);
    m_interfaceItems.add(x);
    m_interfaceItems.add(y);
    m_interfaceItems.add(z);
    m_interfaceItems.add(volume);
    m_interfaceItems.add(pitch);
    m_interfaceItems.add(looping);
    m_interfaceItems.add(play);
    m_interfaceItems.add(stop);
    m_interfaceItems.add(pause);
    m_interfaceItems.add(onEvent);
}

void Sound3D::addToWorld()
{
    m_world.get3DSounds()->addObject(shared_from_this());
}

void Sound3D::loaded() {}
void Sound3D::destroying() {}

void Sound3D::fireEvent(const std::string& type)
{
    Object::fireEvent(onEvent, type);
}
