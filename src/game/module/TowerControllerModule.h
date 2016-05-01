////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_TOWERCONTROLLERMODULE_INCLUDED
#define REAPER_TOWERCONTROLLERMODULE_INCLUDED

#include "game/ModuleUpdater.h"

#include "DamageModule.h"
#include "PositionModule.h"
#include "TeamModule.h"
#include "WeaponModule.h"

struct TowerControllerModuleDescriptor : public ModuleDescriptor {
    f32 rotationSpeed;
    f32 rangeMin;
    f32 rangeMax;
};

struct TowerControllerModule {
    f32 rotationSpeed;
    f32 rangeMin;
    f32 rangeMax;
};

class TowerControllerUpdater : public ModuleUpdater<TowerControllerModule, TowerControllerModuleDescriptor>
{
public:
    TowerControllerUpdater(AbstractWorldUpdater* worldUpdater) : ModuleUpdater<TowerControllerModule, TowerControllerModuleDescriptor>(worldUpdater) {}

public:
    void update(float dt, ModuleAccessor<PositionModule> positionModuleAccessor,
                          ModuleAccessor<WeaponModule> weaponModuleAccessor,
                          ModuleAccessor<DamageModule> damageModuleAccessor,
                          ModuleAccessor<TeamModule> teamModuleAccessor);
    void createModule(EntityId id, const TowerControllerModuleDescriptor* descriptor) override;
};

#endif // REAPER_TOWERCONTROLLERMODULE_INCLUDED
