#pragma once

#include "../../core/object.hpp"
#include "../../core/objectproperty.tpp"
#include "../../core/objectvectorproperty.tpp"

class World;
class ObjectList;

class Sound3D : public Object, public std::enable_shared_from_this<Sound3D>
{
public:
    static constexpr const char* defaultId = "3dSound";

    static std::shared_ptr<Sound3D> create(World& world, std::string_view _id);

    Sound3D(World& world, std::string_view _id);

    void addToWorld();
    void loaded();
    void destroying();

    // Properties
    ObjectProperty<std::string> name;
    ObjectProperty<float> x;
    ObjectProperty<float> y;
    ObjectProperty<float> z;
    ObjectProperty<float> volume;
    ObjectProperty<float> pitch;
    ObjectProperty<bool> looping;

    // Methods
    Method play;
    Method stop;
    Method pause;

    // Event
    Event onEvent;

private:
    void fireEvent(const std::string& type);
};

