////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_STACKTRACE_INCLUDED
#define REAPER_STACKTRACE_INCLUDED

void printStacktrace();

#include <vector>
#include <string>

/*
 * Base stacktrace class, could be used to make an uniform API between platforms
 */
class StackTrace
{
public:
    StackTrace();

public:
    void    pushFrame(unsigned int pc, const std::string& symbol, unsigned int offset);
    const std::string& operator[](std::size_t idx) const;

private:
    std::vector<std::string>   _trace;
};

#endif // REAPER_STACKTRACE_INCLUDED
