#include "BasicHeader.hlsli"

Output BasicVS(
float4 pos : POSITION,
float4 normal : NORMAL,
float2 uv : TEXCOORD,
min16uint2 boneno : BONE_NO,
min16uint weight : WEIGHT
)
{
    
    Output output;
    
    pos = mul(world, pos);
    output.ray = normalize(pos.xyz - eye);
    output.svpos = mul(mul(viewproj, world), pos);
    normal.w = 0; // 平行移動成分を無効にする
    output.normal = mul(world, normal); //法線にもワールド座標変換を行う
    output.vnormal = mul(view, output.normal);
    output.uv = uv;
    return output;
}