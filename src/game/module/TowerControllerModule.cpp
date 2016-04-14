////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TowerControllerModule.h"

#include "math/BasicMath.h"

static void fireWeapon(WeaponModule* weaponModule, DamageModule* targetDamageModule)
{
    if (weaponModule->cooldownMs == 0)
    {
        weaponModule->cooldownMs = weaponModule->rateInvMs;
        targetDamageModule->damages += weaponModule->damage;
    }
}

void TowerControllerUpdater::update(float dt,
                                    ModuleAccessor<PositionModule> positionModuleAccessor,
                                    ModuleAccessor<WeaponModule> weaponModuleAccessor,
                                    ModuleAccessor<DamageModule> damageModuleAccessor,
                                    ModuleAccessor<TeamModule> teamModuleAccessor)
{
    for (auto& controllerModuleIt : _modules)
    {
        auto& moduleInstance = controllerModuleIt.second;

        const EntityId towerEntityId = controllerModuleIt.first;
        PositionModule* towerPositionModule = positionModuleAccessor[towerEntityId];
        const glm::vec2 towerDirection = glm::vec2(cos(towerPositionModule->orientation.x), sin(towerPositionModule->orientation.x));

        for (auto positionModuleIt : positionModuleAccessor.map())
        {
            PositionModule const* targetPositionModule = &positionModuleIt.second;
            const EntityId targetId = positionModuleIt.first;
            const glm::vec2 positionDiff = glm::vec2(targetPositionModule->position - towerPositionModule->position);
            const float distanceSq = sqr(positionDiff.x) + sqr(positionDiff.y);
            const float rangeSq = sqr(moduleInstance.range);

            // Check if target is in range
            if (distanceSq > rangeSq)
                continue;

            // Check if target is an enemy
            if (teamModuleAccessor[towerEntityId]->teamId == teamModuleAccessor[targetId]->teamId)
                continue;

            // Aim at it
            const float firingDirection = atan2f(positionDiff.y, positionDiff.x);
            const float deltaAngle = deltaAngle2D(towerDirection, positionDiff);

            // Are we aligned ?
            if (fabs(deltaAngle) < moduleInstance.rotationSpeed * dt)
            {
                towerPositionModule->orientation.x = firingDirection;
                fireWeapon(weaponModuleAccessor[towerEntityId], damageModuleAccessor[targetId]);
            }
            else
                towerPositionModule->orientation.x += moduleInstance.rotationSpeed * dt * glm::sign(deltaAngle); // Angle can be out of range, but we don't care
        }
    }
}

void TowerControllerUpdater::createModule(EntityId id, const TowerControllerModuleDescriptor* descriptor)
{
    TowerControllerModule module;
    module.range = descriptor->range;
    module.rotationSpeed = descriptor->rotationSpeed;

    addModule(id, module);
}
