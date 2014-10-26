////////////////////////////////////////////////////////////////////////////////
/// GLShaderDev
///
/// Copyright (c) 2014 Thibault Schueller
/// This file is distributed under the GPLv3 License
///
/// @file Log.cpp
/// @author Thibault Schueller <ryp.sqrt@gmail.com>
////////////////////////////////////////////////////////////////////////////////

#ifndef GLSD_LOG_INCLUDED
#define GLSD_LOG_INCLUDED

#include <string>
#include <iostream>
#include <sstream>

class LogLine
{
public:
    LogLine(std::ostream& out = std::cout)
    : _out(out)
    {}

    LogLine(const char* file, const char* func, int line, std::ostream& out = std::cout)
    : _out(out)
    {
        _stream << file << ':' << line << ": in '" << func << "': ";
    }

    ~LogLine()
    {
        _stream << std::endl;
        _out << _stream.rdbuf(); // NOTE C++11 garantees thread safety
        _out.flush();
    }

    template <class T>
    LogLine& operator<<(const T& arg)
    {
        _stream << arg;
        return (*this);
    }

private:
    std::stringstream   _stream;
    std::ostream&       _out;
};

#define LOGEXT()    LogLine(__FILE__, __FUNCTION__, __LINE__)
#define LOG()       LogLine()

#endif // GLSD_LOG_INCLUDED
