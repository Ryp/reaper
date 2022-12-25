////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "LinuxController.h"

#include <core/Assert.h>
#include <core/Platform.h>
#include <core/Types.h>
#include <fmt/format.h>

#include <vector>

#if defined(REAPER_PLATFORM_LINUX)
#    include <fcntl.h>
#    include <linux/joystick.h>
#    include <sys/ioctl.h>
#    include <unistd.h>
#endif

#define VERBOSE_DEBUG 0

namespace Reaper
{
static constexpr int   AxisAbsoluteResolution = (2 << 14) - 1;
static constexpr float AxisDeadzone = 0.08f; // FIXME Don't use dead zones on all axes

LinuxController create_controller(const char* device_path)
{
    LinuxController controller = {};
#if defined(REAPER_PLATFORM_LINUX)
    controller.fd = open(device_path, O_RDONLY | O_NONBLOCK);
    Assert(controller.fd != -1, fmt::format("Couldn't connect to controller {}", device_path));

    char deviceName[256];
    Assert(ioctl(controller.fd, JSIOCGNAME(256), deviceName) != -1, "could not retrieve device name");

    // Assert(false, fmt::format("New controller = {}", deviceName));

    // FIXME Should get the initial state with JS_EVENT_INIT instead
    controller.last_state = update_controller_state(controller);
#endif
    return controller;
}

void destroy_controller(LinuxController& controller)
{
#if defined(REAPER_PLATFORM_LINUX)
    Assert(close(controller.fd) != -1);
#endif
}

struct ControllerButtonRemapEntry
{
    GenericButton::Type generic_button;
};

struct ControllerAxisRemapEntry
{
    GenericAxis::Type generic_axis;
};

struct RemapTable
{
    std::vector<ControllerButtonRemapEntry> buttons;
    std::vector<ControllerAxisRemapEntry>   axes;
};

namespace
{
    static const RemapTable X1RemapTable = {{
                                                {GenericButton::A},
                                                {GenericButton::B},
                                                {GenericButton::X},
                                                {GenericButton::Y},
                                                {GenericButton::LB},
                                                {GenericButton::RB},
                                                {GenericButton::View},
                                                {GenericButton::Menu},
                                                {GenericButton::Xbox},
                                                {GenericButton::LSB},
                                                {GenericButton::RSB},
                                            },
                                            {
                                                {GenericAxis::LSX},
                                                {GenericAxis::LSY},
                                                {GenericAxis::LT},
                                                {GenericAxis::RSX},
                                                {GenericAxis::RSY},
                                                {GenericAxis::RT},
                                                {GenericAxis::DPadX},
                                                {GenericAxis::DPadY},
                                            }};

    static const RemapTable DS4RemapTable = {{
                                                 {GenericButton::A},    // Cross
                                                 {GenericButton::B},    // Circle
                                                 {GenericButton::Y},    // Triangle
                                                 {GenericButton::X},    // Square
                                                 {GenericButton::LB},   // L1
                                                 {GenericButton::RB},   // R1
                                                 {GenericButton::LT},   // L2
                                                 {GenericButton::RT},   // R2
                                                 {GenericButton::View}, // Share
                                                 {GenericButton::Menu}, // Options
                                                 {GenericButton::Xbox}, // PS
                                                 {GenericButton::LSB},  // L3
                                                 {GenericButton::RSB},  // R3
                                             },
                                             {
                                                 {GenericAxis::LSX},   // LSX
                                                 {GenericAxis::LSY},   // LSY
                                                 {GenericAxis::LT},    // L2Pressure
                                                 {GenericAxis::RSY},   // RightAnalogY
                                                 {GenericAxis::RSX},   // RightAnalogX
                                                 {GenericAxis::RT},    // R2Pressure
                                                 {GenericAxis::DPadX}, // DPadHorizontal
                                                 {GenericAxis::DPadY}, // DPadVertical
                                             }};

    namespace Controllers
    {
        enum Type
        {
            XboxOne,
            DualShock4,
            Count
        };
    }

    static const std::array<RemapTable, Controllers::Count> RemapTables = {
        X1RemapTable,
        DS4RemapTable,
    };

} // namespace

GenericControllerState update_controller_state(LinuxController& controller)
{
    GenericControllerState state = controller.last_state;
    const RemapTable&      remap_table = RemapTables[Controllers::DualShock4];
#if defined(REAPER_PLATFORM_LINUX)
    float           val;
    struct js_event event;

    while (read(controller.fd, static_cast<void*>(&event), sizeof(event)) != -1)
    {
        event.type &= ~JS_EVENT_INIT;
        if (event.type == JS_EVENT_AXIS)
        {
            const u32 axis_index = event.number;
            val = static_cast<float>(event.value) / static_cast<float>(AxisAbsoluteResolution);

            if (fabs(val) < AxisDeadzone)
            {
                val = 0.f;
            }

            const float axis_value = val;

            if (axis_index < remap_table.axes.size())
            {
                const ControllerAxisRemapEntry& remap_entry = remap_table.axes[axis_index];

                if (remap_entry.generic_axis != GenericAxis::Unknown)
                {
                    state.axes[remap_entry.generic_axis] = axis_value;
#    if VERBOSE_DEBUG
                    fmt::print("Axis index {} remapped to '{}'\n", axis_index,
                               generic_axis_name_to_string(remap_entry.generic_axis));
                }
                else
                {
                    fmt::print("Axis index {} ignored\n", axis_index);
#    endif
                }
            }
            else
            {
                Assert(false, fmt::format("Unknown axis index = {}", axis_index));
            }
        }
        else if (event.type == JS_EVENT_BUTTON)
        {
            const u32  button_index = event.number;
            const bool button_held = (event.value != 0);

            if (button_index < remap_table.buttons.size())
            {
                const ControllerButtonRemapEntry& remap_entry = remap_table.buttons[button_index];

                if (remap_entry.generic_button != GenericButton::Unknown)
                {
                    state.buttons[remap_entry.generic_button].held = button_held;
#    if VERBOSE_DEBUG
                    fmt::print("Button index {} remapped to '{}'\n", button_index,
                               generic_button_name_to_string(remap_entry.generic_button));
                }
                else
                {
                    fmt::print("Button index {} ignored\n", button_index);
#    endif
                }
            }
            else
            {
                Assert(false, fmt::format("Unknown button index = {}", button_index));
            }
        }
    }
#endif

    compute_controller_transient_state(controller.last_state, state);

    controller.last_state = state;

    return state;
}
} // namespace Reaper
