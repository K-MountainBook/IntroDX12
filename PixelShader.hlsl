#include "BasicHeader.hlsli"

float4 BasicPS(Output input) : SV_TARGET
{
    
    float3 light = normalize(float3(1, -1, 1));
    float brightness = dot(-light, input.normal.xyz);
    
    
    float2 normalUV = input.vnormal.xy;
    normalUV = (normalUV + float2(1, -1)) * float2(0.5, -0.5);
    
    float4 color = tex.Sample(smp, input.uv);
    
    return float4(brightness, brightness, brightness, 1)
        // テクスチャが出るようになる
        // * tex.Sample(smp, input.uv)
        * diffuse // マテリアル
        * tex.Sample(smp, input.uv) // テクスチャ
        * sph.Sample(smp, normalUV) // スフィア
        + spa.Sample(smp, normalUV) // スペキュラ
        + float4(color.xyz * ambient * 0.3, 1); // 環境光
    //return float4(tex.Sample(smp, input.uv));
    // return float4(input.uv, 1, 1);
    // return float4(1, 1, 0, 1);
}