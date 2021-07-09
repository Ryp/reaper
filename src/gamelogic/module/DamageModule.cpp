////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "DamageModule.h"

#include "gamelogic/WorldUpdater.h"
#include "math/BasicMath.h"

void DamageUpdater::update(float /*dt*/)
{
    for (auto& it : _modules)
    {
        auto& moduleInstance = it.second;
        moduleInstance.health = decrement(moduleInstance.health, moduleInstance.damages);
        moduleInstance.damages = 0;

        if (moduleInstance.health == 0)
            _worldUpdater->notifyRemoveEntity(it.first);
    }
}

void DamageUpdater::createModule(EntityId id, const DamageModuleDescriptor* descriptor)
{
    DamageModule module;
    module.maxHealth = descriptor->maxHealth;
    module.health = descriptor->maxHealth;
    module.damages = 0;

    addModule(id, module);
}
