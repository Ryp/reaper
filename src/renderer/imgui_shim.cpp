////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2026 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// See imgui_shim.h. Everything here is a direct forward to the vendored imgui;
// there is no logic in this file on purpose.

#include "imgui_shim.h"

#include <imgui.h>

#include <backends/imgui_impl_vulkan.h>

// The Zig side hardcodes these values (it cannot see the C++ enums), so pin
// them here.
static_assert(RIMGUI_COND_FIRST_USE_EVER == ImGuiCond_FirstUseEver, "imgui enum drift");
static_assert(RIMGUI_SLIDER_FLAGS_LOGARITHMIC == ImGuiSliderFlags_Logarithmic, "imgui enum drift");
static_assert(RIMGUI_INPUT_TEXT_FLAGS_READ_ONLY == ImGuiInputTextFlags_ReadOnly, "imgui enum drift");

extern "C" {
void rimgui_create_context(void)
{
    ImGui::CreateContext();
}
void rimgui_destroy_context(void)
{
    ImGui::DestroyContext();
}

bool rimgui_vulkan_init(const RImGuiVulkanInitInfo* info)
{
    ImGui_ImplVulkan_InitInfo init_info = {
        .Instance = static_cast<VkInstance>(info->instance),
        .PhysicalDevice = static_cast<VkPhysicalDevice>(info->physical_device),
        .Device = static_cast<VkDevice>(info->device),
        .QueueFamily = info->queue_family,
        .Queue = static_cast<VkQueue>(info->queue),
        .PipelineCache = VK_NULL_HANDLE,
        .DescriptorPool = reinterpret_cast<VkDescriptorPool>(info->descriptor_pool),
        .Subpass = 0,
        .MinImageCount = info->min_image_count,
        .ImageCount = info->image_count,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .Allocator = nullptr,
        .CheckVkResultFn = nullptr,
    };

    return ImGui_ImplVulkan_Init(&init_info, static_cast<VkFormat>(info->color_attachment_format));
}

void rimgui_vulkan_shutdown(void)
{
    ImGui_ImplVulkan_Shutdown();
}
void rimgui_vulkan_new_frame(void)
{
    ImGui_ImplVulkan_NewFrame();
}

bool rimgui_vulkan_create_fonts_texture(void* command_buffer)
{
    return ImGui_ImplVulkan_CreateFontsTexture(static_cast<VkCommandBuffer>(command_buffer));
}

void rimgui_vulkan_destroy_font_upload_objects(void)
{
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void rimgui_vulkan_render_draw_data(void* command_buffer)
{
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(command_buffer));
}

void rimgui_new_frame(void)
{
    ImGui::NewFrame();
}
void rimgui_render(void)
{
    ImGui::Render();
}

void rimgui_io_set_display_size(float width, float height)
{
    ImGui::GetIO().DisplaySize = ImVec2(width, height);
}

void rimgui_io_add_mouse_pos_event(float x, float y)
{
    ImGui::GetIO().AddMousePosEvent(x, y);
}
void rimgui_io_add_mouse_button_event(int button, bool down)
{
    ImGui::GetIO().AddMouseButtonEvent(button, down);
}
void rimgui_io_add_mouse_wheel_event(float x, float y)
{
    ImGui::GetIO().AddMouseWheelEvent(x, y);
}

void rimgui_get_main_viewport_work_pos(float* out_x, float* out_y)
{
    const ImVec2 work_pos = ImGui::GetMainViewport()->WorkPos;
    *out_x = work_pos.x;
    *out_y = work_pos.y;
}

void rimgui_set_next_window_pos(float x, float y, int cond)
{
    ImGui::SetNextWindowPos(ImVec2(x, y), cond);
}
void rimgui_set_next_window_bg_alpha(float alpha)
{
    ImGui::SetNextWindowBgAlpha(alpha);
}
bool rimgui_begin(const char* name, bool* p_open)
{
    return ImGui::Begin(name, p_open);
}
void rimgui_end(void)
{
    ImGui::End();
}

bool rimgui_begin_child(const char* str_id, float width, float height)
{
    return ImGui::BeginChild(str_id, ImVec2(width, height));
}

void rimgui_end_child(void)
{
    ImGui::EndChild();
}
void rimgui_same_line(float offset_from_start_x)
{
    ImGui::SameLine(offset_from_start_x);
}
void rimgui_separator(void)
{
    ImGui::Separator();
}

void rimgui_text_unformatted(const char* text)
{
    ImGui::TextUnformatted(text);
}
bool rimgui_button(const char* label)
{
    return ImGui::Button(label);
}
bool rimgui_checkbox(const char* label, bool* v)
{
    return ImGui::Checkbox(label, v);
}
void rimgui_begin_disabled(bool disabled)
{
    ImGui::BeginDisabled(disabled);
}
void rimgui_end_disabled(void)
{
    ImGui::EndDisabled();
}

bool rimgui_slider_float(const char* label, float* v, float v_min, float v_max, const char* format, int flags)
{
    return ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
}

bool rimgui_slider_int(const char* label, int* v, int v_min, int v_max)
{
    return ImGui::SliderInt(label, v, v_min, v_max);
}

bool rimgui_slider_u32(const char* label, uint32_t* v, uint32_t v_min, uint32_t v_max, const char* format)
{
    return ImGui::SliderScalar(label, ImGuiDataType_U32, v, &v_min, &v_max, format);
}

bool rimgui_input_float3(const char* label, float* v, const char* format, int flags)
{
    return ImGui::InputFloat3(label, v, format, flags);
}

void rimgui_get_cursor_screen_pos(float* out_x, float* out_y)
{
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    *out_x = pos.x;
    *out_y = pos.y;
}

void rimgui_draw_rect_filled(float x0, float y0, float x1, float y1, uint32_t color)
{
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color);
}

void rimgui_draw_rect(float x0, float y0, float x1, float y1, uint32_t color)
{
    ImGui::GetWindowDrawList()->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), color);
}

void rimgui_draw_line(float x0, float y0, float x1, float y1, uint32_t color, float thickness)
{
    ImGui::GetWindowDrawList()->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), color, thickness);
}

void rimgui_draw_circle(float cx, float cy, float radius, uint32_t color, int num_segments)
{
    ImGui::GetWindowDrawList()->AddCircle(ImVec2(cx, cy), radius, color, num_segments);
}

void rimgui_draw_circle_filled(float cx, float cy, float radius, uint32_t color)
{
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(cx, cy), radius, color);
}
}
