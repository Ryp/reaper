#ifndef ACONTROLLER_HPP
#define ACONTROLLER_HPP

#include <vector>

class AController
{
public:
  AController(int buttons, int axes);
  virtual ~AController();

public:
  virtual void	update();

public:
  void		reset();

public:
  bool		isHeld(unsigned idx) const;
  bool		isPressed(unsigned idx) const;
  bool		isReleased(unsigned idx) const;
  float		getAxis(unsigned idx) const;

protected:
  typedef struct {
      bool held;
      bool new_held;
      bool pressed;
      bool released;
  } Button;
  typedef float Axe;

protected:
  std::vector<Button>	_buttons;
  std::vector<Axe>	_axes;
};

#endif // ACONTROLLER_HPP
