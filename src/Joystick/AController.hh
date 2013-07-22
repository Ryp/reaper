#ifndef ACONTROLLER_HH
#define ACONTROLLER_HH

class AController
{
public:
  AController();
  virtual ~AController();

public:
  virtual void	update() = 0;

public:
  float		getRotation(int axis) const;

public:
  void		reset();

protected:
  float		_rotation[3];
  float		_linear[3];
};

#endif // ACONTROLLER_HH
