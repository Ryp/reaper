////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_WEAPONMODULE_INCLUDED
#define REAPER_WEAPONMODULE_INCLUDED

#include "game/ModuleUpdater.h"

struct WeaponModuleDescriptor : public ModuleDescriptor {
    u32 damage;
    f32 rate;
};

struct WeaponModule {
    u32 damage;
    u32 rateInvMs;
    u32 cooldownMs;
};

class WeaponUpdater : public ModuleUpdater<WeaponModule, WeaponModuleDescriptor>
{
public:
    WeaponUpdater(AbstractWorldUpdater* worldUpdater) : ModuleUpdater<WeaponModule, WeaponModuleDescriptor>(worldUpdater) {}

public:
    void update(float dt);
    void createModule(EntityId id, const WeaponModuleDescriptor* descriptor) override;
};

#endif // REAPER_WEAPONMODULE_INCLUDED
