#ifndef SIXAXIS_HH
#define SIXAXIS_HH

#include <string>
#include "AController.hh"

class SixAxis : public AController
{
  enum Axes {

  };

  static const int AxisAbsoluteResolution = 2 << 15;
public:
  SixAxis(const std::string& device);
  virtual ~SixAxis();

public:
  virtual void update();

private:
  int	_fd;
  int	_axisX;
  int	_axisZ;
};

#endif // SIXAXIS_HH
