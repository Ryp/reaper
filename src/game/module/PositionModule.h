////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_POSITIONMODULE_INCLUDED
#define REAPER_POSITIONMODULE_INCLUDED

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#include "game/ModuleUpdater.h"

struct PositionModuleDescriptor : public ModuleDescriptor {
    glm::vec3   position;
    glm::vec2   orientation;
};

struct PositionModule {
    glm::vec3   position;
    glm::vec3   linearSpeed;
    glm::vec2   orientation;
    glm::vec2   rotationSpeed;
};

class PositionUpdater : public ModuleUpdater<PositionModule, PositionModuleDescriptor>
{
    using parent_type = ModuleUpdater<PositionModule, PositionModuleDescriptor>;
public:
    PositionUpdater(AbstractWorldUpdater* worldUpdater) : parent_type(worldUpdater) {}

public:
    void update(float dt);
    void createModule(EntityId id, const PositionModuleDescriptor* descriptor) override;
};

#endif // REAPER_POSITIONMODULE_INCLUDED
