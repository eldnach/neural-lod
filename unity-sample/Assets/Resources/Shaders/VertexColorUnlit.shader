Shader "Custom/VertexColorUnlit"
{
    Properties
    {
        _DebugColor ("Debug Color", Color) = (1, 0, 0, 1)
        // This creates a checkbox in the Inspector that toggles the keyword
        [Toggle(_DEBUG_LODS)] _UseDebugLods ("Enable Debug Mode", Float) = 0
    }
    SubShader
    {
        Tags { "RenderType"="Opaque" "RenderPipeline"="UniversalPipeline" }
        LOD 100

        Pass
        {
            HLSLPROGRAM
            #pragma vertex vert
            #pragma fragment frag

            #pragma shader_feature _DEBUG_LODS
            
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "Packages/com.unity.render-pipelines.core/ShaderLibrary/Color.hlsl"

            struct Attributes
            {
                float4 positionOS : POSITION;
                float4 color      : COLOR; 
            };

            struct Varyings
            {
                float4 positionHCS : SV_POSITION;
                float4 color       : COLOR;
            };

            Varyings vert(Attributes IN)
            {
                Varyings OUT;
                OUT.positionHCS = TransformObjectToHClip(IN.positionOS.xyz);
                OUT.color = IN.color;
                return OUT;
            }

            CBUFFER_START(UnityPerMaterial)
                float4 _MainTex_ST;
                float4 _DebugColor;
            CBUFFER_END

            half4 frag(Varyings IN) : SV_Target
            {
                float4 color = float4(1,1,1,1);
                // Applying the conversion to f ix the "washed out" look
                float3 linearColor = SRGBToLinear(IN.color.rgb);
                color = float4(linearColor, IN.color.a);

                #if defined(_DEBUG_LODS)
                    color = _DebugColor;
                #endif
                
                return color;
            }
            ENDHLSL
        }
    }
}