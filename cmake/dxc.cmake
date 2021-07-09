#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2021 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

# NOTE: it is possible to run DXC under linux using wine. If the binary is
# executable, CMake should be able to call it successfully.
# Just make sure you set $DXC_PATH in your env.
find_program(DXC_EXEC dxc HINTS
    "$ENV{DXC_PATH}/build/bin")

if(NOT DXC_EXEC)
    message(FATAL_ERROR "${DXC_EXEC} not found")
endif()

function(dxc_get_shader_model_string shader_type_extension shader_model_version_major shader_model_version_minor output_variable)
    if(shader_type_extension STREQUAL ".comp")
        set(SM_TYPE cs)
    elseif(shader_type_extension STREQUAL ".vert")
        set(SM_TYPE vs)
    elseif(shader_type_extension STREQUAL ".geom")
        set(SM_TYPE gs)
    elseif(shader_type_extension STREQUAL ".tesc")
        set(SM_TYPE hs)
    elseif(shader_type_extension STREQUAL ".tese")
        set(SM_TYPE ds)
    elseif(shader_type_extension STREQUAL ".frag")
        set(SM_TYPE ps)
    else()
        message(FATAL_ERROR "${shader_type_extension} is not a recognized extension")
    endif()

    set(${output_variable} "${SM_TYPE}_${shader_model_version_major}_${shader_model_version_minor}" PARENT_SCOPE)
endfunction()
