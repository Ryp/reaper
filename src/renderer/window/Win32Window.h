////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
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

    void map() override final;
    void unmap() override final;
    void pumpEvents(std::vector<Window::Event>& eventOutput) override final;

public:
    HINSTANCE m_instance;
    HWND      m_handle;
};
} // namespace Reaper
