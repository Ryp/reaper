////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Windows.h>

#include "Window.h"

namespace Reaper
{
class REAPER_RENDERER_API Win32Window : public IWindow
{
public:
    Win32Window(const WindowCreationDescriptor& creationInfo);
    ~Win32Window();

    bool renderLoop(AbstractRenderer* renderer) override;

public:
    HINSTANCE Instance;
    HWND      Handle;
};
} // namespace Reaper
