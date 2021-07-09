////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "WeaponModule.h"

#include "math/BasicMath.h"

void WeaponUpdater::update(float dt)
{
    const u32 dtMs = static_cast<u32>(dt * 1000.0f);

    for (auto& it : _modules)
    {
        WeaponModule& moduleInstance = it.second;
        moduleInstance.cooldownMs = decrement(moduleInstance.cooldownMs, dtMs);
    }
}

void WeaponUpdater::createModule(EntityId id, const WeaponModuleDescriptor* descriptor)
{
    WeaponModule module;
    module.damage = descriptor->damage;
    module.rateInvMs = static_cast<u32>(1000.0f / descriptor->rate);
    module.cooldownMs = 0;

    addModule(id, module);
}
