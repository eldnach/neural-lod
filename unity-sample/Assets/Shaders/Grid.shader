Shader "Custom/RadialGrid_Transparent_Fade"
{
    Properties
    {
        _MainColor ("Grid Color", Color) = (1, 1, 1, 1)
        _LineThickness ("Line Thickness", Range(0.01, 1.0)) = 0.05
        _CircleSpacing ("Circle Spacing", Float) = 10.0
        _SpokeCount ("Spoke Count", Float) = 8.0
        _GridOpacity ("Master Opacity", Range(0, 1)) = 1.0
        
        [Header(Distance Fade)]
        _FadeStart ("Fade Start Distance", Float) = 20.0
        _FadeEnd ("Fade End Distance", Float) = 100.0
    }
    SubShader
    {
        Tags { "Queue"="Transparent" "RenderType"="Transparent" }
        ZWrite Off
        Blend SrcAlpha OneMinusSrcAlpha 

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"

            struct appdata {
                float4 vertex : POSITION;
                float2 uv : TEXCOORD0;
            };

            struct v2f {
                float2 uv : TEXCOORD0;
                float4 vertex : SV_POSITION;
                float3 worldPos : TEXCOORD1; // We need world position for distance
            };

            fixed4 _MainColor;
            float _LineThickness;
            float _CircleSpacing;
            float _SpokeCount;
            float _GridOpacity;
            float _FadeStart;
            float _FadeEnd;

            v2f vert (appdata v) {
                v2f o;
                o.vertex = UnityObjectToClipPos(v.vertex);
                o.uv = v.uv;
                // Calculate world position
                o.worldPos = mul(unity_ObjectToWorld, v.vertex).xyz;
                return o;
            }

            fixed4 frag (v2f i) : SV_Target {
                // 1. Calculate Distance Fade
                // Get distance from camera to this pixel
                float distToCam = distance(i.worldPos, _WorldSpaceCameraPos);
                
                // Create a 1 -> 0 mask based on distance
                float distanceFade = saturate((_FadeEnd - distToCam) / (_FadeEnd - _FadeStart));

                // 2. Grid Logic (Same as before)
                float2 centerUV = i.uv - 0.5;
                float dist = length(centerUV);
                
                // Seamless Spokes
                float angle = atan2(centerUV.y, centerUV.x);
                float spokeValue = cos(angle * _SpokeCount);
                float spokeDelta = fwidth(spokeValue);
                float spokeLine = smoothstep(spokeDelta, 0, abs(spokeValue - 1.0) - (_LineThickness * 0.01));

                // Circles AA
                float circleVal = frac(dist * _CircleSpacing);
                float circleDelta = fwidth(dist * _CircleSpacing);
                float circleLine = smoothstep(circleDelta, 0, abs(circleVal - 0.5) - (_LineThickness * 0.01));

                float gridMask = saturate(circleLine + spokeLine);

                // 3. Final Output
                // Multiply the grid mask by the distance fade
                float finalAlpha = gridMask * _MainColor.a * _GridOpacity * distanceFade;
                
                return fixed4(_MainColor.rgb, finalAlpha);
            }
            ENDCG
        }
    }
}