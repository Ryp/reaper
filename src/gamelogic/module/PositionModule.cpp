////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "PositionModule.h"

void PositionUpdater::update(float dt)
{
    for (auto& it : _modules)
    {
        auto& positionInstance = it.second;
        positionInstance.position += positionInstance.linearSpeed * dt;
        positionInstance.orientation += positionInstance.rotationSpeed * dt;
    }
}

void PositionUpdater::createModule(EntityId id, const PositionModuleDescriptor* descriptor)
{
    PositionModule module;
    module.position = descriptor->position;
    module.orientation = descriptor->orientation;

    addModule(id, module);
}
