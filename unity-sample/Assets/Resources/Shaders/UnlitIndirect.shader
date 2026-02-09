Shader "Unlit/UnlitIndirect"
{
    Properties
    {
        _Colormap("Main Texture", 2D) = "white" {}
    }
    SubShader
    {
        Tags { "RenderType" = "Opaque" "RenderPipeline" = "UniversalPipeline" }
        LOD 100
        Cull Off

        Pass
        {
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma target 4.5

            #pragma shader_feature _DEBUG_LODS

            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"

            struct vertdata
            {
                uint vid : SV_VertexID;
                uint iid : SV_InstanceID;
            };

            struct v2f
            {
                float4 vertex : SV_POSITION;
                float2 uv : TEXCOORD0;
                float4 color : COLOR;
            };

            // Texture Declarations
            TEXTURE2D(_Colormap);
            SAMPLER(sampler_Colormap);

            // Buffers from C#
            StructuredBuffer<float3> vertexBuffer;
            StructuredBuffer<float2> uvBuffer;
            StructuredBuffer<float4> colorBuffer;;
            StructuredBuffer<float4> culledPositionsBuffer;
            float modelHeight;
            float4 lodLevel;

            float4 lod0Color;
            float4 lod1Color;
            float4 lod2Color;
            float4 lod3Color;
            float4 lod4Color;

            v2f vert(vertdata v)
            {
                v2f o;

                float3 vPos = vertexBuffer[v.vid];
                o.uv = uvBuffer[v.vid]; 
                o.color = colorBuffer[v.vid];
                
                float3 instanceOffset = culledPositionsBuffer[v.iid].xyz; 
                float3 worldPos = vPos + instanceOffset; 
                worldPos = TransformObjectToWorld(worldPos);
                o.vertex = TransformWorldToHClip(worldPos); 

                return o; 
            }

            float4 frag(v2f i) : SV_Target
            {
                float4 color = i.color;

                #if defined(_DEBUG_LODS)
                    if (lodLevel.x < 0.25)
                        color = lod0Color;
                    else if (lodLevel.x < 0.5)
                        color = lod1Color;
                    else if (lodLevel.x < 0.75)
                        color = lod2Color;
                    else if (lodLevel.x < 1.0)
                        color = lod3Color;
                    else    
                        color = lod4Color;
                #endif

                return color;
            }
            ENDHLSL
        }
    }
}