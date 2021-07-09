////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include "gamelogic/ModuleUpdater.h"

struct PositionModuleDescriptor : public ModuleDescriptor
{
    glm::vec3 position;
    glm::vec2 orientation;
};

struct PositionModule
{
    glm::vec3 position;
    glm::vec3 linearSpeed;
    glm::vec2 orientation;
    glm::vec2 rotationSpeed;
};

class PositionUpdater : public ModuleUpdater<PositionModule, PositionModuleDescriptor>
{
    using parent_type = ModuleUpdater<PositionModule, PositionModuleDescriptor>;

public:
    PositionUpdater(AbstractWorldUpdater* worldUpdater)
        : parent_type(worldUpdater)
    {}

public:
    void update(float dt);
    void createModule(EntityId id, const PositionModuleDescriptor* descriptor) override;
};
