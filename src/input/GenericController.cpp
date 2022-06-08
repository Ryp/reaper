////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "GenericController.h"

#include <core/Assert.h>
#include <core/Types.h>

namespace Reaper
{
void compute_controller_transient_state(const GenericControllerState& last_state, GenericControllerState& new_state)
{
    for (u32 i = 0; i < GenericButton::Count; i++)
    {
        const ButtonState& last_button_state = last_state.buttons[i];
        ButtonState&       new_button_state = new_state.buttons[i];

        new_button_state.pressed = !last_button_state.held && new_button_state.held;
        new_button_state.released = last_button_state.held && !new_button_state.held;
    }
}

const char* generic_button_name_to_string(GenericButton::Type button)
{
    switch (button)
    {
    case GenericButton::A:
        return "A";
    case GenericButton::B:
        return "B";
    case GenericButton::X:
        return "X";
    case GenericButton::Y:
        return "Y";
    case GenericButton::LB:
        return "LB";
    case GenericButton::RB:
        return "RB";
    case GenericButton::LT:
        return "LT";
    case GenericButton::RT:
        return "RT";
    case GenericButton::LSB:
        return "LSB";
    case GenericButton::RSB:
        return "RSB";
    case GenericButton::View:
        return "View";
    case GenericButton::Menu:
        return "Menu";
    case GenericButton::Xbox:
        return "Xbox";
    default:
        AssertUnreachable();
        break;
    }
    return "Unknown";
}

const char* generic_axis_name_to_string(GenericAxis::Type axis)
{
    switch (axis)
    {
    case GenericAxis::LT:
        return "LT";
    case GenericAxis::RT:
        return "RT";
    case GenericAxis::LSX:
        return "LSX";
    case GenericAxis::LSY:
        return "LSY";
    case GenericAxis::RSX:
        return "RSX";
    case GenericAxis::RSY:
        return "RSY";
    case GenericAxis::DPadX:
        return "DPadX";
    case GenericAxis::DPadY:
        return "DPadY";
    default:
        AssertUnreachable();
        break;
    }
    return "Unknown";
}
} // namespace Reaper
