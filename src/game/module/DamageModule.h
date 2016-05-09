////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_DAMAGEMODULE_INCLUDED
#define REAPER_DAMAGEMODULE_INCLUDED

#include "game/ModuleUpdater.h"

struct DamageModuleDescriptor : public ModuleDescriptor {
    u32 maxHealth;
};

struct DamageModule {
    u32 health;
    u32 maxHealth;
    u32 damages;
};

class DamageUpdater : public ModuleUpdater<DamageModule, DamageModuleDescriptor>
{
    using parent_type = ModuleUpdater<DamageModule, DamageModuleDescriptor>;
public:
    DamageUpdater(AbstractWorldUpdater* worldUpdater) : parent_type(worldUpdater) {}

public:
    void update(float dt);
    void createModule(EntityId id, const DamageModuleDescriptor* descriptor) override;
};

#endif // REAPER_DAMAGEMODULE_INCLUDED
