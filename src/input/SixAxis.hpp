#ifndef SIXAXIS_HPP
#define SIXAXIS_HPP

#include <string>
#include "AController.hpp"

/*
 * NOTE The axis DPadLeftPressure seems correct but won't generate any event
 * (tested on USB with several controllers)
 */

class SixAxis : public AController
{
public:
    enum Buttons {
        Select = 0,
        LeftAnalog,
        RightAnalog,
        Start,
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
        Home,
        UnknownB17,
        UnknownB18,
    };

    enum Axes {
        LeftAnalogX = 0,
        LeftAnalogY,
        RightAnalogX,
        RightAnalogY,
        UnknownA4,
        UnknownA5,
        UnknownA6,
        UnknownA7,
        DPadUpPressure,
        DPadRightPressure,
        DPadDownPressure,
        DPadLeftPressure,
        L2Pressure,
        R2Pressure,
        L1Pressure,
        R1Pressure,
        TrianglePressure,
        CirclePressure,
        CrossPressure,
        SquarePressure,
        UnknownA20,
        UnknownA21,
        UnknownA22,
        Roll,
        Pitch,
        UnknownA25,
        UnknownA26,
    };

    static const int    AxisAbsoluteResolution = (2 << 14) - 1;
    static const int    TotalButtonsNumber = 19;
    static const int    TotalAxesNumber = 27;
    static const float  AxisDeadzone;

public:
    SixAxis(const std::string& device);
    virtual ~SixAxis() = default;

public:
    virtual void update() override;
    virtual void destroy() override;

private:
    bool    connect();

private:
    std::string _device;
    bool        _connected;
    int         _fd;
};

#endif // SIXAXIS_HPP
