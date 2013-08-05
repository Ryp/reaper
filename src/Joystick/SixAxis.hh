#ifndef SIXAXIS_HH
#define SIXAXIS_HH

#include <string>
#include "AController.hh"

class SixAxis : public AController
{
public:
  enum Buttons {
    Start = 0,
    LeftAnalogStick,
    RightAnalogStick,
    Select,
    DPadUp,
    DPadRight,
    DPadDown,
    DPadLeft,
    L2,
    R2,
    L1,
    R1,
    Triangle,
    Circle,
    Cross,
    Square,
    Home
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
