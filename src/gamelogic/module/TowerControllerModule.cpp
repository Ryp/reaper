////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "TowerControllerModule.h"

#include "math/BasicMath.h"

void TowerControllerUpdater::update(float dt,
                                    ModuleAccessor<PositionModule>
                                        positionModuleAccessor,
                                    ModuleAccessor<WeaponModule>
                                        weaponModuleAccessor,
                                    ModuleAccessor<DamageModule>
                                        damageModuleAccessor,
                                    ModuleAccessor<TeamModule>
                                        teamModuleAccessor)
{
    for (auto& controllerModuleIt : _modules)
    {
        auto&           moduleInstance = controllerModuleIt.second;
        const EntityId  towerEntityId = controllerModuleIt.first;
        PositionModule* towerPositionModule = positionModuleAccessor[towerEntityId];
        const glm::vec2 towerDirection =
            glm::vec2(cos(towerPositionModule->orientation.x), sin(towerPositionModule->orientation.x));
        bool      targetFound = false;
        glm::vec2 positionDiff;

        // Try to aim at our previous target if we have one
        if (moduleInstance.activeTargetId != invalidEId())
        {
            positionDiff = glm::vec2(positionModuleAccessor[moduleInstance.activeTargetId]->position
                                     - towerPositionModule->position);

            if (!isPositionInRange(moduleInstance, positionDiff))
                moduleInstance.activeTargetId = invalidEId();
            else
                targetFound = true;
        }

        if (!targetFound)
        {
            // Fallback by searching all neighbours
            for (auto positionModuleIt : positionModuleAccessor.map())
            {
                PositionModule const* targetPositionModule = &positionModuleIt.second;
                const EntityId        targetId = positionModuleIt.first;

                positionDiff = glm::vec2(targetPositionModule->position - towerPositionModule->position);
                if (!isPositionInRange(moduleInstance, positionDiff))
                    continue;

                // Check if target is an enemy
                if (teamModuleAccessor[towerEntityId]->teamId == teamModuleAccessor[targetId]->teamId)
                    continue;

                moduleInstance.activeTargetId = targetId;
                targetFound = true;
            }
        }

        if (targetFound)
        {
            // Aim at it
            const float firingDirection = atan2f(positionDiff.y, positionDiff.x);
            const float deltaAngle = deltaAngle2D(towerDirection, positionDiff);

            // Are we aligned ?
            if (fabs(deltaAngle) < moduleInstance.rotationSpeed * dt)
            {
                towerPositionModule->orientation.x = firingDirection;
                fireWeapon(weaponModuleAccessor[towerEntityId], damageModuleAccessor[moduleInstance.activeTargetId]);
            }
            else
                towerPositionModule->orientation.x +=
                    moduleInstance.rotationSpeed * dt
                    * glm::sign(deltaAngle); // Angle can be out of range, but we don't care
        }
    }
}

void TowerControllerUpdater::createModule(EntityId id, const TowerControllerModuleDescriptor* descriptor)
{
    TowerControllerModule module;
    module.rangeMin = descriptor->rangeMin;
    module.rangeMax = descriptor->rangeMax;
    module.rotationSpeed = descriptor->rotationSpeed;
    module.activeTargetId = invalidEId();

    Assert(module.rangeMin > 0.f, "minimum range must be strictly positive");
    Assert(module.rangeMin < module.rangeMax, "maximum range is lower than minimum range");

    addModule(id, module);
}

void TowerControllerUpdater::removeModule(EntityId id)
{
    parent_type::removeModule(id);

    // Remove the entity from the active targets
    for (auto& controllerModuleIt : _modules)
    {
        auto& moduleInstance = controllerModuleIt.second;

        if (moduleInstance.activeTargetId == id)
            moduleInstance.activeTargetId = invalidEId();
    }
}

void TowerControllerUpdater::fireWeapon(WeaponModule* weaponModule, DamageModule* targetDamageModule)
{
    if (weaponModule->cooldownMs == 0)
    {
        weaponModule->cooldownMs = weaponModule->rateInvMs;
        targetDamageModule->damages += weaponModule->damage;
    }
}

bool TowerControllerUpdater::isPositionInRange(const TowerControllerModule& controllerModule,
                                               const glm::vec2&             relativePosition)
{
    const float distanceSq = sqr(relativePosition.x) + sqr(relativePosition.y);
    const float rangeMinSq = sqr(controllerModule.rangeMin);
    const float rangeMaxSq = sqr(controllerModule.rangeMax);

    if (distanceSq > rangeMaxSq || distanceSq < rangeMinSq)
        return false;
    return true;
}
