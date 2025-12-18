#pragma once
#include "../../core/idobject.hpp"
#include "../../core/objectproperty.tpp"
#include "../../core/objectvectorproperty.tpp"
#include "../../world/world.hpp"
#include <string>

class 3dSound : public IdObject
{
public:
    3dSound(World& world, std::string_view _id);

    // Properties
    Property<std::string> name;
    Property<float> x;
    Property<float> y;
    Property<float> z;
    Property<float> volume;
    Property<float> pitch;

    void addToWorld();
    void loaded();
    void destroying();

    // Simple event example
    Event<> onPlay;

    // Fire event
    void play();
};
