////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Camera.h"

#include <cmath>

#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>

const float Camera::PitchDeadzone = 0.001f;

Camera::Camera(const glm::vec3& pos, const glm::vec3& direction, float yaw, float pitch)
:   _viewMatrix(glm::mat4(1.0)),
    _position(pos),
    _direction(direction),
    _yaw(yaw),
    _pitch(pitch)
{
    update(Cartesian);
}

Camera::~Camera() {}

void Camera::update(Mode mode)
{
    if (mode == Spherical)
        _direction = glm::vec3(cos(_yaw) * cos(_pitch), sin(_pitch), sin(_yaw) * cos(_pitch));
    _viewMatrix = glm::lookAt(_position, _position + _direction * 10.0f, glm::vec3(0, 1, 0));
}

void Camera::reset()
{
    _yaw = 0.0f;
    _pitch = 0.0f;
    update(Spherical);
}

glm::mat4& Camera::getViewMatrix()
{
    return (_viewMatrix);
}

void Camera::setDirection(const glm::vec3& direction)
{
    _direction = glm::normalize(direction);
}

void Camera::setRotation(float yaw, float pitch)
{
    _yaw = fmod(yaw, glm::pi<float>() * 2.0f);
    _pitch = fmod(pitch + glm::half_pi<float>(), glm::pi<float>()) - glm::half_pi<float>();
    _direction = glm::vec3(cos(_yaw) * cos(_pitch), sin(_pitch), sin(_yaw) * cos(_pitch));
}

void Camera::rotate(float yaw, float pitch)
{
    float newPitch = _pitch + pitch;
    if (newPitch < (-glm::half_pi<float>() + PitchDeadzone))
        _pitch = -glm::half_pi<float>() + PitchDeadzone;
    else if (newPitch > (glm::half_pi<float>() - PitchDeadzone))
        _pitch = glm::half_pi<float>() - PitchDeadzone;
    else
        _pitch = newPitch;
    _yaw = fmod(_yaw + yaw, glm::pi<float>() * 2.0f);
}

void Camera::setPosition(const glm::vec3& position)
{
    _position = position;
}

void Camera::translate(const glm::vec3& v)
{
    _position += v;
}

void Camera::move(float front, float lateral, float up)
{
    glm::vec3 side = _direction;

    _position += _direction * front;
    side[1] = side[0];
    side[0] = side[2];
    side[2] = -side[1];
    side[1] = 0.0f;
    side = glm::normalize(side);
    _position += side * lateral;
    _position += glm::vec3(0.0f, 1.0f, 0.0f) * up;
}
