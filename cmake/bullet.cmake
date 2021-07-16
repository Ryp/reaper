#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

# Find actual dependency
find_package(Bullet REQUIRED)

# Wrap bullet for easier consumption
add_library(bullet INTERFACE)

target_include_directories(bullet SYSTEM INTERFACE ${BULLET_INCLUDE_DIRS})
target_link_libraries(bullet INTERFACE ${BULLET_LIBRARIES})
