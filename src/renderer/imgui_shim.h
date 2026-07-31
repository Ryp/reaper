////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2026 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

// A plain-C surface over the subset of Dear ImGui the renderer uses.
//
// Dear ImGui's API is C++ (namespaces, references, default arguments, member
// functions), and Zig's @cImport only understands C — so the Zig side talks to
// the vendored imgui through this file instead. Every entry point is a
// one-liner over the real call; the only judgement here is which calls to
// expose, which is exactly the set used by GuiPass.cpp, TestGraphics.cpp's
// backend_debug_ui() and GameLoop.cpp's three debug windows.
//
// Vulkan handles cross this boundary as raw values rather than Vulkan types, so
// that including this header does not drag the whole Vulkan C API into the Zig
// translation unit alongside the vulkan-zig bindings.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mirrors of the imgui enum values used at the call sites. The .cpp
// static_asserts each one against the real enum, so they cannot drift.
#define RIMGUI_COND_FIRST_USE_EVER 4
#define RIMGUI_SLIDER_FLAGS_LOGARITHMIC 32
#define RIMGUI_INPUT_TEXT_FLAGS_READ_ONLY 16384

typedef struct RImGuiVulkanInitInfo
{
    void*    instance;        // VkInstance
    void*    physical_device; // VkPhysicalDevice
    void*    device;          // VkDevice
    uint32_t queue_family;
    void*    queue;           // VkQueue
    uint64_t descriptor_pool; // VkDescriptorPool
    uint32_t min_image_count;
    uint32_t image_count;
    int32_t  color_attachment_format; // VkFormat
} RImGuiVulkanInitInfo;

// ---- Context lifetime ----
void rimgui_create_context(void);
/// Scales the UI for a high-DPI display. Must be called after
/// rimgui_create_context() and BEFORE the font atlas is uploaded, because it
/// rebuilds the default font at the scaled size rather than magnifying the
/// bitmap one -- upscaling ProggyClean is legible but visibly soft.
void rimgui_apply_dpi_scale(float scale);
void rimgui_destroy_context(void);

// ---- Vulkan backend ----
bool rimgui_vulkan_init(const RImGuiVulkanInitInfo* info);
void rimgui_vulkan_shutdown(void);
void rimgui_vulkan_new_frame(void);
bool rimgui_vulkan_create_fonts_texture(void* command_buffer);
void rimgui_vulkan_destroy_font_upload_objects(void);
/// Draws whatever the last rimgui_render() produced. Fetches the draw data
/// internally, mirroring ExecuteFrame.cpp's ImGui::GetDrawData() hand-off.
void rimgui_vulkan_render_draw_data(void* command_buffer);

// ---- Frame ----
void rimgui_new_frame(void);
void rimgui_render(void);

// ---- IO ----
void rimgui_io_set_display_size(float width, float height);
void rimgui_io_add_mouse_pos_event(float x, float y);
void rimgui_io_add_mouse_button_event(int button, bool down);
void rimgui_io_add_mouse_wheel_event(float x, float y);

// ---- Windows and layout ----
void rimgui_get_main_viewport_work_pos(float* out_x, float* out_y);
void rimgui_set_next_window_pos(float x, float y, int cond);
void rimgui_set_next_window_bg_alpha(float alpha);
bool rimgui_begin(const char* name, bool* p_open);
void rimgui_end(void);
bool rimgui_begin_child(const char* str_id, float width, float height);
void rimgui_end_child(void);
void rimgui_same_line(float offset_from_start_x);
void rimgui_separator(void);

// ---- Widgets ----
void rimgui_text_unformatted(const char* text);
bool rimgui_button(const char* label);
bool rimgui_checkbox(const char* label, bool* v);
void rimgui_begin_disabled(bool disabled);
void rimgui_end_disabled(void);
bool rimgui_slider_float(const char* label, float* v, float v_min, float v_max, const char* format, int flags);
bool rimgui_slider_int(const char* label, int* v, int v_min, int v_max);
bool rimgui_slider_u32(const char* label, uint32_t* v, uint32_t v_min, uint32_t v_max, const char* format);
bool rimgui_input_float3(const char* label, float* v, const char* format, int flags);

// ---- Raw draw list, for the gamepad axis widgets ----
void rimgui_get_cursor_screen_pos(float* out_x, float* out_y);
void rimgui_draw_rect_filled(float x0, float y0, float x1, float y1, uint32_t color);
void rimgui_draw_rect(float x0, float y0, float x1, float y1, uint32_t color);
void rimgui_draw_line(float x0, float y0, float x1, float y1, uint32_t color, float thickness);
void rimgui_draw_circle(float cx, float cy, float radius, uint32_t color, int num_segments);
void rimgui_draw_circle_filled(float cx, float cy, float radius, uint32_t color);

#ifdef __cplusplus
}
#endif
