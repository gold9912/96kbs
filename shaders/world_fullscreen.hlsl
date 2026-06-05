struct PSIn {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float hash21(float2 p) {
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float roomSdf(float2 p, float2 halfSize) {
    float2 d = abs(p) - halfSize;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float4 MainPS(PSIn input) : SV_Target0 {
    float2 uv = input.uv * 2.0 - 1.0;
    float2 roomP = uv * float2(9.0, 5.0);
    float room = roomSdf(roomP, float2(7.25, 3.75));
    float wall = smoothstep(0.08, -0.04, room);
    float runes = step(0.94, hash21(floor(roomP * 5.0)));
    float fog = saturate(1.0 - length(uv) * 0.85);
    float3 floorCol = lerp(float3(0.025, 0.018, 0.022), float3(0.12, 0.035, 0.05), fog);
    float3 runeCol = float3(0.9, 0.17, 0.08) * runes * wall;
    float3 col = floorCol * wall + runeCol;
    col += float3(0.03, 0.02, 0.035) * smoothstep(-0.5, 0.2, -room);
    return float4(col, 1.0);
}

