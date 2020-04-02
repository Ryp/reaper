struct CPUData
{
    // Should result in 'ColMajor' in SPIR-V
    // https://github.com/Microsoft/DirectXShaderCompiler/blob/master/docs/SPIR-V.rst#packing
    row_major float4x4 my_matrix;
};

[[vk::binding(0, 0)]] StructuredBuffer<CPUData> structured_buffer_data;
[[vk::binding(1, 0)]] ConstantBuffer<CPUData> constant_buffer_data;

[[vk::binding(2, 0)]] RWByteAddressBuffer Out;

[numthreads(1, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID,
          uint3 gid  : SV_GroupID,
          uint3 dtid : SV_DispatchThreadID,
          uint  gi   : SV_GroupIndex)
{
    const float4 test0 = mul(structured_buffer_data[gi].my_matrix, float4(gid, 1.0));
    const float4 test1 = mul(constant_buffer_data.my_matrix, float4(gid, 1.0)); // wrong result

    Out.Store4(0, test0);
    Out.Store4(1, test1);
}

// With glslang:
// ...
// OpMemberDecorate %CPUData 0 ColMajor
// OpMemberDecorate %CPUData 0 Offset 0
// OpMemberDecorate %CPUData 0 MatrixStride 16
// ...
// OpMemberDecorate %constant_buffer_data 0 RowMajor // NO!
// OpMemberDecorate %constant_buffer_data 0 Offset 0
// OpMemberDecorate %constant_buffer_data 0 MatrixStride 16
// ...

// With dxc:
// ...
// OpMemberDecorate %CPUData 0 Offset 0
// OpMemberDecorate %CPUData 0 MatrixStride 16
// OpMemberDecorate %CPUData 0 ColMajor
// ...
// OpMemberDecorate %type_ConstantBuffer_CPUData 0 Offset 0
// OpMemberDecorate %type_ConstantBuffer_CPUData 0 MatrixStride 16
// OpMemberDecorate %type_ConstantBuffer_CPUData 0 ColMajor
// ...
