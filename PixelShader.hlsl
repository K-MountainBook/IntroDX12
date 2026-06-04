#include "BasicHeader.hlsli"

float4 BasicPS(Output input) : SV_TARGET
{
    
    float3 light = normalize(float3(1, -1, 1));
    float brightness = dot(-light, input.normal);
    
    float4 texColor = tex.Sample(smp, input.uv);
    
    return float4(brightness, brightness, brightness, 1)
        // テクスチャが出るようになる
        // * tex.Sample(smp, input.uv)
        * diffuse
        * texColor
    ;
    //return float4(tex.Sample(smp, input.uv));
    // return float4(input.uv, 1, 1);
    // return float4(1, 1, 0, 1);
}