////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

void print_stacktrace_safe();

#include <string>
#include <vector>

/*
 * Base stacktrace class, could be used to make an uniform API between platforms
 */
class StackTrace
{
public:
    StackTrace();

public:
    void               pushFrame(unsigned int pc, const std::string& symbol, unsigned int offset);
    const std::string& operator[](std::size_t idx) const;

private:
    std::vector<std::string> _trace;
};
