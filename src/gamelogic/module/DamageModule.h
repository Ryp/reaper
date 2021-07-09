////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "gamelogic/ModuleUpdater.h"

struct DamageModuleDescriptor : public ModuleDescriptor
{
    u32 maxHealth;
};

struct DamageModule
{
    u32 health;
    u32 maxHealth;
    u32 damages;
};

class DamageUpdater : public ModuleUpdater<DamageModule, DamageModuleDescriptor>
{
    using parent_type = ModuleUpdater<DamageModule, DamageModuleDescriptor>;

public:
    DamageUpdater(AbstractWorldUpdater* worldUpdater)
        : parent_type(worldUpdater)
    {}

public:
    void update(float dt);
    void createModule(EntityId id, const DamageModuleDescriptor* descriptor) override;
};
