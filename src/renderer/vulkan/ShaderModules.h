////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2022 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <unordered_map>
#include <vulkan_loader/Vulkan.h>

#include <span>
#include <unordered_map>
#include <vector>

namespace Reaper
{
struct ShaderModules
{
    struct CodeSpan
    {
        std::size_t offset_bytes;
        std::size_t size_bytes;
    };

    // All spans refer to this vector
    std::vector<char>                         code;
    std::unordered_map<std::string, CodeSpan> code_map;
};

struct ReaperRoot;

void create_shader_modules(ShaderModules& shader_modules, ReaperRoot& root);
void destroy_shader_modules(ShaderModules& shader_modules);

std::span<const u32> get_spirv_shader_module(const ShaderModules& shader_modules, const char* file_name);
} // namespace Reaper
