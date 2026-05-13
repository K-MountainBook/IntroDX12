#include "BasicShaderHeader.hlsli"


float4 BasicPS(Output input) : SV_TARGET
{
    // return float4(tex.Sample(smp,input.uv));
    // return float4(0.9f, 0.4f, 0.9f, 1);
    return float4(input.normal.xyz, 1);
}