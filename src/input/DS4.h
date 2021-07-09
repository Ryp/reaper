////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>

#include "InputExport.h"

#include "AbstractController.h"

class REAPER_INPUT_API DS4 : public AbstractController
{
public:
    enum Buttons
    {
        Select = 0,
        LeftAnalog,
        RightAnalog,
        Square,
        L1,
        DPadRight,
        DPadDown,
        DPadLeft,
        L2,
        R2,
        UnknownB10,
        R1,
        Triangle,
        Circle,
        Cross,
        UnknownB15,
        Home,
        UnknownB17,
        UnknownB18,
    };

    enum Axes
    {
        LeftAnalogX = 0,
        LeftAnalogY,
        L2Pressure,
        RightAnalogY,
        RightAnalogX,
        R2Pressure,
        DPadHorizontal,
        DPadVertical,
    };

    static const int   AxisAbsoluteResolution = (2 << 14) - 1;
    static const int   TotalButtonsNumber = 19;
    static const int   TotalAxesNumber = 8;
    static const float AxisDeadzone;

public:
    DS4(const std::string& device);
    ~DS4() = default;

public:
    virtual void update() override;
    virtual void destroy() override;

private:
    bool connect();

private:
    std::string _device;
    bool        _connected;
    int         _fd;
};
