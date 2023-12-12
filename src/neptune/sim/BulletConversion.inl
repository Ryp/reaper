////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2023 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <btBulletDynamicsCommon.h>

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

// IMPORTANT NOTE:
// - GLM has col-major accessors
// - btMatrix has row-major accessors
namespace Neptune
{
// btVector3 <-> glm::vec3
inline btVector3 toBt(glm::vec3 const& vec)
{
    return btVector3(vec.x, vec.y, vec.z);
}

inline glm::vec3 toGlm(btVector3 const& vec)
{
    return glm::vec3(vec.getX(), vec.getY(), vec.getZ());
}

// btTransform <-> glm::mat3x3
inline btMatrix3x3 toBt(const glm::mat3x3& transform)
{
    return btMatrix3x3(toBt(glm::row(transform, 0)), toBt(glm::row(transform, 1)), toBt(glm::row(transform, 2)));
}

inline glm::mat3x3 toGlm(const btMatrix3x3& transform)
{
    return glm::mat3x3(toGlm(transform.getColumn(0)), toGlm(transform.getColumn(1)), toGlm(transform.getColumn(2)));
}

// btTransform <-> glm::mat4x3
inline btTransform toBt(const glm::mat4x3& transform)
{
    return btTransform(
        btMatrix3x3(toBt(glm::row(transform, 0)), toBt(glm::row(transform, 1)), toBt(glm::row(transform, 2))),
        toBt(glm::column(transform, 3)));
}

inline glm::mat4x3 toGlm(const btTransform& transform)
{
    return glm::mat4x3(toGlm(transform.getBasis().getColumn(0)),
                       toGlm(transform.getBasis().getColumn(1)),
                       toGlm(transform.getBasis().getColumn(2)),
                       toGlm(transform.getOrigin()));
}

// btQuaternion <-> glm::quat
inline btQuaternion toBt(glm::quat const& quat)
{
    return btQuaternion(quat.w, quat.x, quat.y, quat.z);
}

inline glm::quat toGlm(btQuaternion const& quat)
{
    return glm::quat(quat.getW(), quat.getX(), quat.getY(), quat.getZ());
}
} // namespace Neptune
