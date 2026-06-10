#include "BasicHeader.hlsli"

float4 BasicPS(Output input) : SV_TARGET
{
    float3 light = normalize(float3(1, -1, 1));
    float3 lightColor = float3(1, 1, 1);
    float diffuseB = dot(-light, input.normal);
    
    float3 refLight = normalize(reflect(light, input.normal.xyz));
    float3 specularB = pow(saturate(dot(refLight, -input.ray)), specular.a);
    
    float2 sphereMapUV = input.vnormal.xy;
    sphereMapUV = (sphereMapUV + float2(1, -1)) * float2(0.5, -0.5);
    
    float4 texColor = tex.Sample(smp, input.uv);
    
    // return float4(specularB * specular.rgb, 1);
    
    return max(
        diffuseB
        * diffuse
        * texColor
        * sph.Sample(smp, sphereMapUV)
        + spa.Sample(smp, sphereMapUV) * texColor
        + float4(specularB * specular.aaa, 1), float4(texColor * ambient, 1)
);

}