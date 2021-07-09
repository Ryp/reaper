////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "gamelogic/ModuleUpdater.h"

#include "DamageModule.h"
#include "PositionModule.h"
#include "TeamModule.h"
#include "WeaponModule.h"

struct TowerControllerModuleDescriptor : public ModuleDescriptor
{
    f32 rotationSpeed;
    f32 rangeMin;
    f32 rangeMax;
};

struct TowerControllerModule
{
    f32      rotationSpeed;
    f32      rangeMin;
    f32      rangeMax;
    EntityId activeTargetId;
};

class TowerControllerUpdater : public ModuleUpdater<TowerControllerModule, TowerControllerModuleDescriptor>
{
    using parent_type = ModuleUpdater<TowerControllerModule, TowerControllerModuleDescriptor>;

public:
    TowerControllerUpdater(AbstractWorldUpdater* worldUpdater)
        : parent_type(worldUpdater)
    {}

public:
    void update(float dt, ModuleAccessor<PositionModule> positionModuleAccessor,
                ModuleAccessor<WeaponModule> weaponModuleAccessor, ModuleAccessor<DamageModule> damageModuleAccessor,
                ModuleAccessor<TeamModule> teamModuleAccessor);
    void createModule(EntityId id, const TowerControllerModuleDescriptor* descriptor) override;
    void removeModule(EntityId id) override;

private:
    static void fireWeapon(WeaponModule* weaponModule, DamageModule* targetDamageModule);
    static bool isPositionInRange(const TowerControllerModule& controllerModule, const glm::vec2& relativePosition);
};
