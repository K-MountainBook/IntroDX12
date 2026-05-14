#include "BasicShaderHeader.hlsli"


float4 BasicPS(Output input) : SV_TARGET
{
    
    float3 light = normalize(float3(1, -1, 1));
    float brightness = dot(-light, input.normal);
    
    return float4(brightness, brightness, brightness, 1) * diffuse;
    
    // return float4(tex.Sample(smp,input.uv));
    // return float4(0.9f, 0.4f, 0.9f, 1);
    // return float4(input.normal.xyz, 1);
}