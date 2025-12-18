#pragma once

// Dependencies
#include "../../../world/getworld.hpp"       // Adjust path to 'world' folder
#include "../../../core/objectlist.hpp"       // Core object list
#include "../../../core/attributes.hpp"       // Attributes, properties, etc.

// Forward declaration if needed
class 3dSound;

// Your class definition
class 3dSoundList {
public:
    // Constructor
    3dSoundList(World& world);

    // Example methods
    void addSound(3dSound* sound);
    void removeSound(3dSound* sound);

private:
    ObjectList<3dSound> sounds; // or whatever container you use
};
