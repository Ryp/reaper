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
#include <glm/gtx/projection.hpp>
#include <glm/vec3.hpp>

namespace Neptune
{
inline glm::vec3 toGlm(btVector3 const& vec)
{
    return glm::vec3(vec.getX(), vec.getY(), vec.getZ());
}

inline glm::quat toGlm(btQuaternion const& quat)
{
    return glm::quat(quat.getW(), quat.getX(), quat.getY(), quat.getZ());
}

inline glm::fmat3x3 toGlm(const btMatrix3x3& basis)
{
    return glm::fmat3x3(toGlm(basis[0]), toGlm(basis[1]), toGlm(basis[2]));
}

inline glm::fmat4x3 toGlm(const btTransform& transform)
{
    const btMatrix3x3 basis = transform.getBasis();

    const glm::fvec3 translation = toGlm(transform.getOrigin());

    return glm::fmat4x3(toGlm(basis[0]), toGlm(basis[1]), toGlm(basis[2]), translation);
}

inline btVector3 toBt(glm::vec3 const& vec)
{
    return btVector3(vec.x, vec.y, vec.z);
}

inline btMatrix3x3 m33toBt(const glm::fmat3x3& m)
{
    return btMatrix3x3(m[0].x, m[1].x, m[2].x, m[0].y, m[1].y, m[2].y, m[0].z, m[1].z, m[2].z);
}

inline btQuaternion toBt(glm::quat const& quat)
{
    return btQuaternion(quat.w, quat.x, quat.y, quat.z);
}

inline btTransform toBt(const glm::fmat4x3& transform)
{
    btMatrix3x3 mat = m33toBt(glm::fmat3x3(transform));
    btTransform ret = btTransform(mat, toBt(glm::column(transform, 3)));

    return ret;
}
} // namespace Neptune
