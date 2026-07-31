// Port of src/renderer/vulkan/Pipeline.h + Pipeline.cpp
//
// Mostly a pile of "default state" constructors. They exist so that call sites
// only spell out the handful of fields they actually care about, which is the
// same reason the C++ has them — keeping them field-for-field identical is what
// makes the ported renderpasses trustworthy.
//
// Shader modules are never created: with maintenance5 a VkShaderModuleCreateInfo
// chained into the stage's pNext is enough.

const std = @import("std");
const vk = @import("vulkan");

pub fn defaultEntryPoint() [*:0]const u8 {
    return "main";
}

// --------------------------------------------------------------------------
// Descriptor set layouts and pipeline layouts
// --------------------------------------------------------------------------

pub fn allocateDescriptorSets(
    vkd: anytype,
    device: vk.Device,
    descriptor_pool: vk.DescriptorPool,
    descriptor_set_layouts: []const vk.DescriptorSetLayout,
    output_descriptor_sets: []vk.DescriptorSet,
) !void {
    std.debug.assert(descriptor_set_layouts.len == output_descriptor_sets.len);

    const alloc_info = vk.DescriptorSetAllocateInfo{
        .s_type = .descriptor_set_allocate_info,
        .p_next = null,
        .descriptor_pool = descriptor_pool,
        .descriptor_set_count = @intCast(descriptor_set_layouts.len),
        .p_set_layouts = descriptor_set_layouts.ptr,
    };

    try vkd.allocateDescriptorSets(device, &alloc_info, output_descriptor_sets.ptr);
}

pub fn descriptorSetLayoutBindingFlagsCreateInfo(
    binding_flags: []const vk.DescriptorBindingFlags,
) vk.DescriptorSetLayoutBindingFlagsCreateInfo {
    return .{
        .s_type = .descriptor_set_layout_binding_flags_create_info,
        .p_next = null,
        .binding_count = @intCast(binding_flags.len),
        .p_binding_flags = binding_flags.ptr,
    };
}

pub fn descriptorSetLayoutCreateInfo(
    layout_bindings: []const vk.DescriptorSetLayoutBinding,
) vk.DescriptorSetLayoutCreateInfo {
    return .{
        .s_type = .descriptor_set_layout_create_info,
        .p_next = null,
        .flags = .{},
        .binding_count = @intCast(layout_bindings.len),
        .p_bindings = layout_bindings.ptr,
    };
}

pub fn createDescriptorSetLayout(
    vkd: anytype,
    device: vk.Device,
    layout_bindings: []const vk.DescriptorSetLayoutBinding,
    binding_flags: []const vk.DescriptorBindingFlags,
) !vk.DescriptorSetLayout {
    var create_info = descriptorSetLayoutCreateInfo(layout_bindings);
    var flags_create_info: vk.DescriptorSetLayoutBindingFlagsCreateInfo = undefined;

    if (binding_flags.len > 0) {
        std.debug.assert(binding_flags.len == layout_bindings.len);

        flags_create_info = descriptorSetLayoutBindingFlagsCreateInfo(binding_flags);
        create_info.p_next = @ptrCast(&flags_create_info);
    }

    return vkd.createDescriptorSetLayout(device, &create_info, null);
}

pub fn createPipelineLayout(
    vkd: anytype,
    device: vk.Device,
    descriptor_set_layouts: []const vk.DescriptorSetLayout,
    push_constant_ranges: []const vk.PushConstantRange,
) !vk.PipelineLayout {
    const create_info = vk.PipelineLayoutCreateInfo{
        .s_type = .pipeline_layout_create_info,
        .p_next = null,
        .flags = .{},
        .set_layout_count = @intCast(descriptor_set_layouts.len),
        .p_set_layouts = descriptor_set_layouts.ptr,
        .push_constant_range_count = @intCast(push_constant_ranges.len),
        .p_push_constant_ranges = push_constant_ranges.ptr,
    };

    return vkd.createPipelineLayout(device, &create_info, null);
}

// --------------------------------------------------------------------------
// Shader stages
// --------------------------------------------------------------------------

pub fn shaderModuleCreateInfo(shader_spirv: []const u32) vk.ShaderModuleCreateInfo {
    return .{
        .s_type = .shader_module_create_info,
        .p_next = null,
        .flags = .{},
        .code_size = shader_spirv.len * @sizeOf(u32),
        .p_code = shader_spirv.ptr,
    };
}

/// `module_create_info` must outlive the pipeline creation call — it is chained
/// into pNext rather than turned into a VkShaderModule (maintenance5).
pub fn defaultPipelineShaderStageCreateInfo(
    stage_bit: vk.ShaderStageFlags,
    module_create_info: *const vk.ShaderModuleCreateInfo,
    specialization_info: ?*const vk.SpecializationInfo,
) vk.PipelineShaderStageCreateInfo {
    return .{
        .s_type = .pipeline_shader_stage_create_info,
        .p_next = @ptrCast(module_create_info),
        .flags = .{},
        .stage = stage_bit,
        .module = .null_handle,
        .p_name = defaultEntryPoint(),
        .p_specialization_info = specialization_info,
    };
}

// --------------------------------------------------------------------------
// Default pipeline state
// --------------------------------------------------------------------------

pub fn defaultPipelineColorBlendAttachmentState() vk.PipelineColorBlendAttachmentState {
    return .{
        .blend_enable = .false,
        .src_color_blend_factor = .one,
        .dst_color_blend_factor = .zero,
        .color_blend_op = .add,
        .src_alpha_blend_factor = .one,
        .dst_alpha_blend_factor = .zero,
        .alpha_blend_op = .add,
        .color_write_mask = .{ .r_bit = true, .g_bit = true, .b_bit = true, .a_bit = true },
    };
}

pub fn defaultPipelineRenderingCreateInfo() vk.PipelineRenderingCreateInfo {
    return .{
        .s_type = .pipeline_rendering_create_info,
        .p_next = null,
        .view_mask = 0,
        .color_attachment_count = 0,
        .p_color_attachment_formats = null,
        .depth_attachment_format = .undefined,
        .stencil_attachment_format = .undefined,
    };
}

pub fn defaultPipelineVertexInputStateCreateInfo() vk.PipelineVertexInputStateCreateInfo {
    return .{
        .s_type = .pipeline_vertex_input_state_create_info,
        .p_next = null,
        .flags = .{},
        .vertex_binding_description_count = 0,
        .p_vertex_binding_descriptions = null,
        .vertex_attribute_description_count = 0,
        .p_vertex_attribute_descriptions = null,
    };
}

pub fn defaultPipelineInputAssemblyStateCreateInfo() vk.PipelineInputAssemblyStateCreateInfo {
    return .{
        .s_type = .pipeline_input_assembly_state_create_info,
        .p_next = null,
        .flags = .{},
        .topology = .triangle_list,
        .primitive_restart_enable = .false,
    };
}

pub fn defaultPipelineViewportStateCreateInfo() vk.PipelineViewportStateCreateInfo {
    return .{
        .s_type = .pipeline_viewport_state_create_info,
        .p_next = null,
        .flags = .{},
        .viewport_count = 1,
        .p_viewports = null, // dynamic viewport
        .scissor_count = 1,
        .p_scissors = null, // dynamic scissor
    };
}

pub fn defaultPipelineRasterizationStateCreateInfo() vk.PipelineRasterizationStateCreateInfo {
    return .{
        .s_type = .pipeline_rasterization_state_create_info,
        .p_next = null,
        .flags = .{},
        .depth_clamp_enable = .false,
        .rasterizer_discard_enable = .false,
        .polygon_mode = .fill,
        .cull_mode = .{ .back_bit = true },
        .front_face = .counter_clockwise,
        .depth_bias_enable = .false,
        .depth_bias_constant_factor = 0.0,
        .depth_bias_clamp = 0.0,
        .depth_bias_slope_factor = 0.0,
        .line_width = 1.0,
    };
}

pub fn defaultPipelineMultisampleStateCreateInfo() vk.PipelineMultisampleStateCreateInfo {
    return .{
        .s_type = .pipeline_multisample_state_create_info,
        .p_next = null,
        .flags = .{},
        .rasterization_samples = .{ .@"1_bit" = true },
        .sample_shading_enable = .false,
        .min_sample_shading = 1.0,
        .p_sample_mask = null,
        .alpha_to_coverage_enable = .false,
        .alpha_to_one_enable = .false,
    };
}

pub fn defaultPipelineDepthStencilStateCreateInfo() vk.PipelineDepthStencilStateCreateInfo {
    return .{
        .s_type = .pipeline_depth_stencil_state_create_info,
        .p_next = null,
        .flags = .{},
        .depth_test_enable = .false,
        .depth_write_enable = .false,
        .depth_compare_op = .less,
        .depth_bounds_test_enable = .false,
        .stencil_test_enable = .false,
        .front = std.mem.zeroes(vk.StencilOpState),
        .back = std.mem.zeroes(vk.StencilOpState),
        .min_depth_bounds = 0.0,
        .max_depth_bounds = 1.0,
    };
}

pub fn defaultPipelineColorBlendStateCreateInfo() vk.PipelineColorBlendStateCreateInfo {
    return .{
        .s_type = .pipeline_color_blend_state_create_info,
        .p_next = null,
        .flags = .{},
        .logic_op_enable = .false,
        .logic_op = .copy,
        .attachment_count = 0,
        .p_attachments = null,
        .blend_constants = .{ 0.0, 0.0, 0.0, 0.0 },
    };
}

/// The mandatory structures for a graphics pipeline, mirroring
/// GraphicsPipelineProperties. createGraphicsPipeline() takes this by pointer
/// because it hands Vulkan interior pointers to these fields.
pub const GraphicsPipelineProperties = struct {
    vertex_input: vk.PipelineVertexInputStateCreateInfo,
    input_assembly: vk.PipelineInputAssemblyStateCreateInfo,
    viewport: vk.PipelineViewportStateCreateInfo,
    raster: vk.PipelineRasterizationStateCreateInfo,
    multisample: vk.PipelineMultisampleStateCreateInfo,
    depth_stencil: vk.PipelineDepthStencilStateCreateInfo,
    blend_state: vk.PipelineColorBlendStateCreateInfo,
    pipeline_layout: vk.PipelineLayout,
    // Technically an extension, but the engine makes it mandatory.
    pipeline_rendering: vk.PipelineRenderingCreateInfo,
};

pub fn defaultGraphicsPipelineProperties(p_next: ?*const anyopaque) GraphicsPipelineProperties {
    var pipeline_rendering = defaultPipelineRenderingCreateInfo();
    pipeline_rendering.p_next = p_next;

    return .{
        .vertex_input = defaultPipelineVertexInputStateCreateInfo(),
        .input_assembly = defaultPipelineInputAssemblyStateCreateInfo(),
        .viewport = defaultPipelineViewportStateCreateInfo(),
        .raster = defaultPipelineRasterizationStateCreateInfo(),
        .multisample = defaultPipelineMultisampleStateCreateInfo(),
        .depth_stencil = defaultPipelineDepthStencilStateCreateInfo(),
        .blend_state = defaultPipelineColorBlendStateCreateInfo(),
        .pipeline_layout = .null_handle,
        .pipeline_rendering = pipeline_rendering,
    };
}

pub fn createPipelineDynamicStateCreateInfo(
    dynamic_states: []const vk.DynamicState,
) vk.PipelineDynamicStateCreateInfo {
    return .{
        .s_type = .pipeline_dynamic_state_create_info,
        .p_next = null,
        .flags = .{},
        .dynamic_state_count = @intCast(dynamic_states.len),
        .p_dynamic_states = dynamic_states.ptr,
    };
}

// --------------------------------------------------------------------------
// Pipeline creation
// --------------------------------------------------------------------------

pub fn createComputePipeline(
    vkd: anytype,
    device: vk.Device,
    pipeline_layout: vk.PipelineLayout,
    shader_stage_create_info: vk.PipelineShaderStageCreateInfo,
) !vk.Pipeline {
    const create_infos = [_]vk.ComputePipelineCreateInfo{.{
        .s_type = .compute_pipeline_create_info,
        .p_next = null,
        .flags = .{},
        .stage = shader_stage_create_info,
        .layout = pipeline_layout,
        .base_pipeline_handle = .null_handle, // do not care about pipeline derivatives
        .base_pipeline_index = 0,
    }};

    var pipeline: vk.Pipeline = .null_handle;
    _ = try vkd.createComputePipelines(device, .null_handle, &create_infos, null, @ptrCast(&pipeline));

    return pipeline;
}

pub fn createGraphicsPipeline(
    vkd: anytype,
    device: vk.Device,
    shader_stages: []const vk.PipelineShaderStageCreateInfo,
    properties: *const GraphicsPipelineProperties,
    dynamic_states: []const vk.DynamicState,
) !vk.Pipeline {
    const dynamic_state = createPipelineDynamicStateCreateInfo(dynamic_states);

    const create_infos = [_]vk.GraphicsPipelineCreateInfo{.{
        .s_type = .graphics_pipeline_create_info,
        .p_next = @ptrCast(&properties.pipeline_rendering),
        .flags = .{},
        .stage_count = @intCast(shader_stages.len),
        .p_stages = shader_stages.ptr,
        .p_vertex_input_state = &properties.vertex_input,
        .p_input_assembly_state = &properties.input_assembly,
        .p_tessellation_state = null,
        .p_viewport_state = &properties.viewport,
        .p_rasterization_state = &properties.raster,
        .p_multisample_state = &properties.multisample,
        .p_depth_stencil_state = &properties.depth_stencil,
        .p_color_blend_state = &properties.blend_state,
        .p_dynamic_state = &dynamic_state,
        .layout = properties.pipeline_layout,
        .render_pass = .null_handle,
        .subpass = 0,
        .base_pipeline_handle = .null_handle,
        .base_pipeline_index = -1,
    }};

    var pipeline: vk.Pipeline = .null_handle;
    _ = try vkd.createGraphicsPipelines(device, .null_handle, &create_infos, null, @ptrCast(&pipeline));

    return pipeline;
}

// --------------------------------------------------------------------------
// Dynamic rendering
// --------------------------------------------------------------------------

pub fn defaultRenderingAttachmentInfo(image_view: vk.ImageView, layout: vk.ImageLayout) vk.RenderingAttachmentInfo {
    return .{
        .s_type = .rendering_attachment_info,
        .p_next = null,
        .image_view = image_view,
        .image_layout = layout,
        .resolve_mode = .{},
        .resolve_image_view = .null_handle,
        .resolve_image_layout = .undefined,
        .load_op = .load,
        .store_op = .store,
        .clear_value = std.mem.zeroes(vk.ClearValue),
    };
}

pub fn defaultRenderingInfo(
    render_rect: vk.Rect2D,
    color_attachments: []const vk.RenderingAttachmentInfo,
    depth_attachment: ?*const vk.RenderingAttachmentInfo,
) vk.RenderingInfo {
    return .{
        .s_type = .rendering_info,
        .p_next = null,
        .flags = .{},
        .render_area = render_rect,
        .layer_count = 1,
        .view_mask = 0,
        .color_attachment_count = @intCast(color_attachments.len),
        .p_color_attachments = color_attachments.ptr,
        .p_depth_attachment = depth_attachment,
        .p_stencil_attachment = null,
    };
}
