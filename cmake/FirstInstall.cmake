#///////////////////////////////////////////////////////////////////////////////
#// Reaper
#//
#// Copyright (c) 2015-2018 Thibault Schueller
#// This file is distributed under the MIT License
#///////////////////////////////////////////////////////////////////////////////

if(NOT WIN32)
    set(DOWNLOAD_DIRECTORY ".")

    # Install LLVM
    set(LLVM_INSTALLER ${DOWNLOAD_DIRECTORY}/LLVM-6.0.0-win64.exe)
    message(STATUS "Downloading ${LLVM_INSTALLER}")
    file(DOWNLOAD "https://releases.llvm.org/6.0.0/LLVM-6.0.0-win64.exe" ${LLVM_INSTALLER}
        EXPECTED_HASH MD5=f9523838bf214db2959c1d88d3b79d4d
        INACTIVITY_TIMEOUT 10)

    message(STATUS "Installing ${LLVM_INSTALLER}")
    execute_process(COMMAND ${LLVM_INSTALLER})

    # Install Vulkan SDK
    set(VULKAN_SDK_INSTALLER ${DOWNLOAD_DIRECTORY}/VulkanSDK-1.1.70.1-Installer.exe)
    message(STATUS "Downloading ${VULKAN_SDK_INSTALLER}")
    file(DOWNLOAD "https://www.dropbox.com/s/b9xyr1q43p2ov7z/VulkanSDK-1.1.70.1-Installer.exe?dl=1" ${VULKAN_SDK_INSTALLER}
        EXPECTED_HASH MD5=d05fc5b8ba6f2bdd85e6ac0b2dbbe181
        INACTIVITY_TIMEOUT 10)

    message(STATUS "Installing ${VULKAN_SDK_INSTALLER}")
    execute_process(COMMAND ${VULKAN_SDK_INSTALLER})
elseif(UNIX)
    message("Nothing to do for this platform yet")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()
