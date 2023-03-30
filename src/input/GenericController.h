////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <array>

#include "InputExport.h"

namespace Reaper
{
// Based on a Xbox One controller
namespace GenericButton
{
    enum Type
    {
        A,
        B,
        X,
        Y,
        LB,
        RB,
        LT, // FIXME handle as trigger and button
        RT, // FIXME handle as trigger and button
        LSB,
        RSB,
        View,
        Menu,
        Xbox,
        Count,
        Unknown = Count
    };
}

namespace GenericAxis
{
    enum Type
    {
        LT,
        RT,
        LSX,
        LSY,
        RSX,
        RSY,
        DPadX,
        DPadY,
        Count,
        Unknown = Count
    };
}

struct ButtonState
{
    bool held;
    bool pressed;
    bool released;
};

using AxisState = float;

struct GenericControllerState
{
    std::array<ButtonState, GenericButton::Count> buttons;
    std::array<AxisState, GenericAxis::Count>     axes;
};

REAPER_INPUT_API GenericControllerState create_generic_controller_state();

// Updates transient fields like 'pressed' or 'released' for this frame
REAPER_INPUT_API void compute_controller_transient_state(const GenericControllerState& last_state,
                                                         GenericControllerState&       new_state);

const char* generic_button_name_to_string(GenericButton::Type button);
const char* generic_axis_name_to_string(GenericAxis::Type axis);

} // namespace Reaper
