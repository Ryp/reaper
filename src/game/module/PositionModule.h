////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_POSITIONMODULE_INCLUDED
#define REAPER_POSITIONMODULE_INCLUDED

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#include "game/ModuleUpdater.h"

struct PositionModuleDescriptor {
    glm::vec3   position;
    glm::vec2   orientation;
};

struct PositionModule {
    glm::vec3   position;
    glm::vec3   linearSpeed;
    glm::vec2   orientation;
    glm::vec2   rotationSpeed;
};

class PositionUpdater : public ModuleUpdater<PositionModule>
{
public:
    PositionUpdater(AbstractWorldUpdater* worldUpdater) : ModuleUpdater<PositionModule>(worldUpdater) {}

public:
    void update(float dt);
    void createModule(EntityId id, PositionModuleDescriptor const* descriptor);
};

#endif // REAPER_POSITIONMODULE_INCLUDED
