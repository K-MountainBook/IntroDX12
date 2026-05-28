#include "BasicHeader.hlsli"

float4 BasicPS(Output input) : SV_TARGET
{
    
    return float4(0, 0, 0, 1);
    //return float4(tex.Sample(smp, input.uv));
    // return float4(input.uv, 1, 1);
    // return float4(1, 1, 0, 1);
}