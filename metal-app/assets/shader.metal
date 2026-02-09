#include <metal_stdlib>

using namespace metal;

struct VertexInputs{
    device float3* pos [[id(0)]];
    device float3* col [[id(1)]];
    device float2* uv [[id(2)]];
};

struct FrameConstants{
    float progress;
};

struct MaterialConstants{
    float matID;
};

struct InstanceConstants{
    float4x4 viewProjMatrix;
    float4x4 worldMatrix;
    float3 color;
};

struct v2f
{
    float4 pos [[position]];
    float3 col;
    float2 uv;
    float id;
    uint slice [[render_target_array_index]];
};

v2f vertex VertexStage(device const VertexInputs* vertexInputs[[buffer(0)]],
                       constant FrameConstants* frameConstants[[buffer(1)]],
                       constant MaterialConstants* materialConstants[[buffer(2)]],
                       constant InstanceConstants* instanceConstants[[buffer(3)]],
                       uint vertID [[vertex_id]],
                       uint instanceID [[instance_id]])
{
    v2f out;
    float4x4 worldMatrix = instanceConstants[instanceID].worldMatrix;
    float4x4 viewProjMatrix = instanceConstants[instanceID].viewProjMatrix;
    
    out.pos = worldMatrix * float4(vertexInputs->pos[vertID], 1.0);
    out.pos = viewProjMatrix * out.pos;
    out.id = materialConstants->matID;
    out.col = instanceConstants[instanceID].color;
    out.col = vertexInputs->col[vertID];
    out.uv = vertexInputs->uv[vertID];
    out.slice = instanceID;

    return out;
}

float4 fragment FragmentStage( v2f varyings [[stage_in]],
                               texture2d<float> tex [[texture(0)]],
                               sampler smplr [[sampler(0)]])
{
    float4 texColr = tex.sample(smplr, varyings.uv);
    return float4(varyings.col, 1.0);
}
