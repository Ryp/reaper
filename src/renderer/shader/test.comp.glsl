#version 450 core

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define GROUP_X 8
#define GROUP_Y 8
#define GROUP_Z 1
#define GROUP_SIZE (GROUP_X * GROUP_Y * GROUP_Z)

layout (local_size_x = GROUP_X,
        local_size_y = GROUP_Y,
        local_size_z = GROUP_Z) in;

shared uint lds_cache[GROUP_Y][GROUP_X];

//layout (std430, binding = 1) buffer output_buffer
//{
//    uint data[GROUP_SIZE];
//};

layout (set = 0, binding = 0) uniform sampler2D sampler_color;

void main()
{
    //in uvec3 gl_NumWorkGroups;
    //in uvec3 gl_LocalInvocationID;    // SV_GroupThreadID
    //in uvec3 gl_WorkGroupID;          // SV_GroupID
    //in uvec3 gl_GlobalInvocationID;   // SV_DispatchThreadID
    //in uint  gl_LocalInvocationIndex; // SV_GroupIndex
    //const uvec3 gl_WorkGroupSize;

    //gl_NumWorkGroups
    //    This variable contains the number of work groups passed to the dispatch function.
    //gl_WorkGroupID
    //    This is the current work group for this shader invocation.
    //    Each of the XYZ components will be on the half-open range [0, gl_NumWorkGroups.XYZ).
    //gl_LocalInvocationID
    //    This is the current invocation of the shader within the work group.
    //    Each of the XYZ components will be on the half-open range [0, gl_WorkGroupSize.XYZ).
    //gl_GlobalInvocationID
    //    This value uniquely identifies this particular invocation of the compute shader
    //    among all invocations of this compute dispatch call. It's a short-hand for the math computation:
    //    gl_WorkGroupID * gl_WorkGroupSize + gl_LocalInvocationID;

    //gl_LocalInvocationIndex
    //    This is a 1D version of gl_LocalInvocationID. It identifies this invocation's index within the work group. It is short-hand for this math computation:
    //    gl_LocalInvocationID.z * gl_WorkGroupSize.x * gl_WorkGroupSize.y +
    //    gl_LocalInvocationID.y * gl_WorkGroupSize.x +
    //    gl_LocalInvocationID.x;

    //const uvec3 idx = gl_LocalInvocationID;

    //lds_cache[idx.y][idx.x] = 0;

    //memoryBarrierShared();
    //barrier();

    //data[gl_LocalInvocationIndex] = lds_cache[7 - idx.y][7 - idx.x];
}
