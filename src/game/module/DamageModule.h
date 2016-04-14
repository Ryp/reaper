////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_DAMAGEMODULE_INCLUDED
#define REAPER_DAMAGEMODULE_INCLUDED

#include "game/ModuleUpdater.h"

struct DamageModuleDescriptor {
    u32 maxHealth;
};

struct DamageModule {
    u32 health;
    u32 maxHealth;
    u32 damages;
};

class DamageUpdater : public ModuleUpdater<DamageModule>
{
public:
    DamageUpdater(AbstractWorldUpdater* worldUpdater) : ModuleUpdater<DamageModule>(worldUpdater) {}

public:
    void update(float dt);
    void createModule(EntityId id, DamageModuleDescriptor const* descriptor);
};

#endif // REAPER_DAMAGEMODULE_INCLUDED
