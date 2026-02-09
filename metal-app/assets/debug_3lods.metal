#include <metal_stdlib>

using namespace metal;

struct FrameConstants{
    float progress;
};

struct v2f {
    float4 position [[position]];
    float2 uv;
};

vertex v2f VertexStage(uint vid [[vertex_id]]) {
    v2f out;
    
    // Create a triangle strip for a quad: (-1,-1), (-1,1), (1,-1), (1,1)
    float2 grid = float2((vid << 1) & 2, vid & 2);
    out.position = float4(grid * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    out.uv = grid; 
    
    return out;
}

fragment float4 FragmentStage(v2f in [[stage_in]],
                              texture2d_array<float> lod0Tex [[texture(0)]],
                              texture2d_array<float> lod1Tex [[texture(1)]],
                              texture2d_array<float> lod2Tex [[texture(2)]],
                              constant FrameConstants* frameConstants[[buffer(0)]])
{
    const float2 gridDims = float2(1.0, 3.0); 
    
    float colFloat = in.uv.x * 3.0;
    uint colIndex = uint(floor(colFloat));
    
    // Remap the UV.x to [0.0, 1.0]
    float2 localUV = in.uv;
    localUV.x = fract(colFloat);
    
    uint2 tileCoord = uint2(localUV * gridDims);
    uint sliceIndex = tileCoord.y * uint(gridDims.x) + tileCoord.x;
    float2 tileUV = fract(localUV * gridDims);
    
    sampler s(address::clamp_to_edge, filter::linear);
    float4 color;

    float4 refColor = lod0Tex.sample(s, tileUV, sliceIndex);

    float4 columnColor;
    if (colIndex == 0)      columnColor = refColor;
    else if (colIndex == 1) columnColor = lod1Tex.sample(s, tileUV, sliceIndex);
    else if (colIndex == 2) columnColor = lod2Tex.sample(s, tileUV, sliceIndex);

    float error = saturate(distance(refColor.rgb, columnColor.rgb) * 2.0);

    // 3. Calculate error relative to LOD 0
    float progress = frameConstants[0].progress;
    float barHeight = 0.05;
    float2 barUV = float2(in.uv.x, 1.0 - in.uv.y);
    if (barUV.y < barHeight) {
        float4 barColor = float4(0.0, 0.0, 0.0, 1.0);
        if (barUV.x < progress) {
            barColor = float4(1.0, 1.0, 1.0, 1.0);
        }
        if (barUV.y > 0.005 && barUV.y < barHeight - 0.005 && barUV.x > 0.002 && barUV.x < 0.998) {
             return barColor;
        }
        return float4(0.0, 0.0, 0.0, 1.0); 
    }

    color = mix(columnColor, float4(0.0, 1.0, 0.0, 1.0), error);
    return color;
}