////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "gamelogic/ModuleUpdater.h"

struct WeaponModuleDescriptor : public ModuleDescriptor
{
    u32 damage;
    f32 rate;
};

struct WeaponModule
{
    u32 damage;
    u32 rateInvMs;
    u32 cooldownMs;
};

class WeaponUpdater : public ModuleUpdater<WeaponModule, WeaponModuleDescriptor>
{
    using parent_type = ModuleUpdater<WeaponModule, WeaponModuleDescriptor>;

public:
    WeaponUpdater(AbstractWorldUpdater* worldUpdater)
        : parent_type(worldUpdater)
    {}

public:
    void update(float dt);
    void createModule(EntityId id, const WeaponModuleDescriptor* descriptor) override;
};
