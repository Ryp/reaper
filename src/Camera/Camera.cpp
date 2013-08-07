#include "Camera.hpp"

#include <cmath>
#include <glm/gtx/transform.hpp>

Camera::Camera(const glm::vec3& pos, const glm::vec3& direction)
  : _viewMatrix(glm::mat4(1.0)),
    _pos(pos),
    _direction(direction)
{
  update();
}

Camera::~Camera() {}

void Camera::update()
{
  _viewMatrix = glm::lookAt(_pos, _pos + _direction, glm::vec3(0, 1, 0));
}

glm::mat4& Camera::getViewMatrix()
{
  return (_viewMatrix);
}

void Camera::setDirection(const glm::vec3& direction)
{
  _direction = direction;
}

void Camera::setRotation(float yaw, float pitch)
{
  yaw = (yaw / 360.0f) * M_PI * 2.0f;
  pitch = (pitch / 360.0f) * M_PI * 2.0f;
  _direction = glm::vec3(cos(yaw) * cos(pitch), sin(pitch), sin(yaw) * cos(pitch));
}

void Camera::rotate(float yaw, float pitch)
{
  yaw = (yaw / 360.0f) * M_PI * 2.0f;
  pitch = (pitch / 360.0f) * M_PI * 2.0f;
  /* TODO */
}

void Camera::setPosition(const glm::vec3& position)
{
  _pos = position;
}

void Camera::translate(const glm::vec3& v)
{
  _pos += v;
}
