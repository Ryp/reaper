////////////////////////////////////////////////////////////////////////////////
/// ReaperGL
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#ifndef REAPER_CVAR_INCLUDED
#define REAPER_CVAR_INCLUDED

#include <string>

// Same as f(n) = pow(2, n - 1) for ints, with f(0) = 0
static constexpr int bit(int n)
{
    return n ? (1 << (n - 1)) : 0;
}

namespace reaper
{
    class CVar
    {
    public:
        enum Flag {
            None    = bit(0),
            Bool    = bit(1),
            Int     = bit(2),
            Float   = bit(3),
            String  = bit(4)
        };

    public:
        CVar(const std::string& name, int flags);
        ~CVar() = default;

        CVar(const CVar& other) = delete;
        CVar& operator=(const CVar& other) = delete;

    public:
        

    private:
        const std::string   _name;
        int                 _flags;
        bool                _valueBool;
        int                 _valueInt;
        float               _valueFloat;
        std::string         _valueStr;
    };
}

#endif // REAPER_CVAR_INCLUDED
