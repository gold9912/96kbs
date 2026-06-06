Texture2D<float4> gDxrOutput : register(t0);
SamplerState gLinearClamp : register(s0);

struct PSIn {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 MainPS(PSIn input) : SV_Target0 {
    float4 dxrSample = gDxrOutput.SampleLevel(gLinearClamp, input.uv, 0.0);
    float3 dxr = dxrSample.rgb;
    float intensity = max(max(dxr.r, dxr.g), dxr.b);
    float alpha = saturate(max(dxrSample.a, intensity * 1.15));
    return float4(dxr, alpha);
}
