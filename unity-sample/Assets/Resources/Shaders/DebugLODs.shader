Shader "DebugLODs"
{
SubShader
{
    Tags { "RenderType"="Opaque" "RenderPipeline" = "UniversalPipeline"}
    ZWrite Off Cull Off
    Pass
    {
        Name "ColorBlitPass"
        HLSLPROGRAM
        #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
        #include "Packages/com.unity.render-pipelines.core/Runtime/Utilities/Blit.hlsl"
        #pragma vertex Vert
        #pragma fragment Frag

        float debugMode;
 
        float4 Frag(Varyings input) : SV_Target0
        {
            UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(input);
            
            // 1. Shift UVs: Subtract 0.5 so the texture starts at the middle of the screen
            float2 uv = input.texcoord.xy - 0.5;

            // 2. Scale UVs: Multiply by 2 because 0.5 (remaining space) * 2 = 1.0 (full texture)
            uv *= 2;

            float zoom = 5.5; 
            float2 uvZoom = (uv - 0.5) / zoom + 0.5;
            half4 texCol = SAMPLE_TEXTURE2D_X_LOD(_BlitTexture, sampler_PointRepeat, uvZoom, 0);

            // 3. Discard everything that isn't in the top-right quadrant
            if (input.texcoord.x < 0.5 || input.texcoord.y < 0.5)
            {
                discard;
            }

            float4 color = float4(texCol.rgb, 1.0);
            
            if (debugMode == 3)
            {
                color = float4(texCol.a, texCol.a, texCol.a, 1.0);
            }
  
            return color;
        }
        ENDHLSL
    }
}
}