#include <metal_stdlib>
using namespace metal;

void atomic_add_float(device atomic_uint* addr, float value) {
    uint expected = atomic_load_explicit(addr, memory_order_relaxed);
    uint desired;
    do {
        desired = as_type<uint>(as_type<float>(expected) + value);
    } while (!atomic_compare_exchange_weak_explicit(addr, &expected, desired, memory_order_relaxed, memory_order_relaxed));
}

kernel void ComputeKernel(
    texture2d_array<float, access::read> lod0Tex [[texture(0)]],
    texture2d_array<float, access::read> lod1Tex [[texture(1)]],
    texture2d_array<float, access::read> lod2Tex [[texture(2)]],
    texture2d_array<float, access::read> lod3Tex [[texture(3)]],
    texture2d_array<float, access::read> lod4Tex [[texture(4)]],
    device atomic_uint* errorBuffer [[buffer(0)]],
    uint3 gid [[thread_position_in_grid]])
{
    uint layer = gid.z;
    uint2 pos = gid.xy * 5;

    // Each 'layer' in the texture2d_array is a separate mesh instance
    float3 color0 = lod0Tex.read(pos, layer).rgb;
    float3 color1 = lod1Tex.read(pos, layer).rgb;
    float3 color2 = lod2Tex.read(pos, layer).rgb;
    float3 color3 = lod3Tex.read(pos, layer).rgb;
    float3 color4 = lod4Tex.read(pos, layer).rgb;
    
    // Calculate Squared Errors
    float err0 = dot(color0 - color0, color0 - color0);
    float err1 = dot(color0 - color1, color0 - color1);
    float err2 = dot(color0 - color2, color0 - color2);
    float err3 = dot(color0 - color3, color0 - color3);
    float err4 = dot(color0 - color4, color0 - color4);
    
    uint baseIdx = layer * 5;
    atomic_add_float(&errorBuffer[baseIdx + 0], err0);
    atomic_add_float(&errorBuffer[baseIdx + 1], err1);
    atomic_add_float(&errorBuffer[baseIdx + 2], err2);
    atomic_add_float(&errorBuffer[baseIdx + 3], err3);
    atomic_add_float(&errorBuffer[baseIdx + 4], err4);
}
