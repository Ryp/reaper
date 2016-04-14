////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "MovementModule.h"

#include "math/BasicMath.h"

void MovementUpdater::update(float dt, ModuleAccessor<PositionModule> positionModuleAccessor)
{
    for (auto& movementModuleIt : _modules)
    {
        auto& moduleInstance = movementModuleIt.second;

        PositionModule* positionModule = positionModuleAccessor[movementModuleIt.first];
        float distanceToTravel = moduleInstance.speed * dt;
        AIPath& path = moduleInstance.path;

        // Consume distance and segments while following the path
        while (distanceToTravel > 0.f && !path.empty())
        {
            const glm::vec2 posToSegmentEnd = path.back().end - glm::vec2(positionModule->position);
            const float segmentRemainingLength = glm::length(posToSegmentEnd);

            if (distanceToTravel > segmentRemainingLength)
            {
                // We can travel further than the current segment
                positionModule->position = glm::vec3(path.back().end, positionModule->position.z);
                distanceToTravel -= segmentRemainingLength;
                path.pop_back();
            }
            else
            {
                // We stay on the same segment
                positionModule->position += glm::vec3(glm::normalize(posToSegmentEnd) * distanceToTravel, 0.f);
                distanceToTravel = 0.f;
            }
            positionModule->orientation = glm::vec2(angle(posToSegmentEnd), positionModule->orientation.y);
        }
    }
}

void MovementUpdater::createModule(EntityId id, const MovementModuleDescriptor* descriptor)
{
    MovementModule module;
    module.speed = descriptor->speed;
    module.path.resize(3);
    module.path[2] = {glm::vec2(1.f, 1.f),  glm::vec2(1.f, 0.f)};
    module.path[1] = {glm::vec2(1.f, 0.f),  glm::vec2(1.f, -1.f)};
    module.path[0] = {glm::vec2(1.f, -1.f), glm::vec2(0.f, -1.f)};

    addModule(id, module);
}
