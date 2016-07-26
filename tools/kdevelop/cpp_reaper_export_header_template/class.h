////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2016 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////


{% block include_guard_open %}
#ifndef REAPER_{{ name|upper }}_EXPORT_INCLUDED
#define REAPER_{{ name|upper }}_EXPORT_INCLUDED
{% endblock include_guard_open %}


#include "core/Compiler.h"


// Make sure this is up to date with the build system.
#if defined(REAPER_BUILD_SHARED)
    #if defined(reaper_{{ name|lower }}_EXPORTS)
        #define REAPER_{{ name|upper }}_API REAPER_EXPORT
    #else
        #define REAPER_{{ name|upper }}_API REAPER_IMPORT
    #endif
#elif defined(REAPER_BUILD_STATIC)
    #define REAPER_{{ name|upper }}_API
#else
    #error Build type must be defined
#endif

{% block include_guard_close %}
#endif // REAPER_{{ name|upper }}_EXPORT_INCLUDED
{% endblock include_guard_close %}
