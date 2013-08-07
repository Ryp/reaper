#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>

class Camera
{
public:
  Camera(const glm::vec3& pos = glm::vec3(0), const glm::vec3& direction = glm::vec3(1, 0, 0));
  virtual ~Camera();

public:
  void	update();

public:
  glm::mat4&	getViewMatrix();

public:
  void	setDirection(const glm::vec3& direction);
  void	setRotation(float yaw, float pitch);
  void	rotate(float yaw, float pitch);
  void	setPosition(const glm::vec3& position);
  void	translate(const glm::vec3& v);

protected:
  glm::mat4	_viewMatrix;
  glm::vec3	_pos;
  glm::vec3	_direction;
};

#endif // CAMERA_HPP
