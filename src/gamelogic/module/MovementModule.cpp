////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
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
        float           distanceToTravel = moduleInstance.speed * dt;
        AIPath&         path = moduleInstance.path;

        // Consume distance and segments while following the path
        while (distanceToTravel > 0.f && !path.empty())
        {
            const glm::vec2 posToSegmentEnd = path.back().end - glm::vec2(positionModule->position);
            const float     segmentRemainingLength = glm::length(posToSegmentEnd);

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

    addModule(id, module);
}

void MovementUpdater::buildAIPath(MovementModule* movementModule, const TDPath& path)
{
    Assert(!path.empty());

    const float       nodeSpacing = 1.f;
    const glm::vec2   nodeOffset(0.f, 0.f);
    const std::size_t segmentsNo = path.size() - 1;
    uvec2             start;
    uvec2             end;

    movementModule->path.resize(segmentsNo);
    for (std::size_t i = 0; i < segmentsNo; ++i)
    {
        start = path[i];
        end = path[i + 1];

        movementModule->path[segmentsNo - i - 1] = {nodeOffset + glm::vec2(start.x, start.y) * nodeSpacing,
                                                    nodeOffset + glm::vec2(end.x, end.y) * nodeSpacing};
    }
}
