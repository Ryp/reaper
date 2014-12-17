#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>

class Camera
{
public:
  enum Mode {
    Spherical,
    Cartesian
  };

  static const float PitchDeadzone;

public:
  Camera(const glm::vec3& position = glm::vec3(0), const glm::vec3& direction = glm::vec3(1, 0, 0), float yaw = 0.0f, float pitch = 0.0f);
  virtual ~Camera();

public:
  void	update(Mode mode);
  void	reset();

public:
  glm::mat4&	getViewMatrix();

public:
  void	            setDirection(const glm::vec3& direction);
  void	            setRotation(float yaw, float pitch);
  void	            rotate(float yaw, float pitch);
  void	            setPosition(const glm::vec3& position);
  void	            translate(const glm::vec3& v);
  void	            move(float front, float lateral, float up);

protected:
  glm::mat4	_viewMatrix;
  glm::vec3	_position;
  glm::vec3	_direction;
  float		_yaw;
  float		_pitch;
};

#endif // CAMERA_HPP
