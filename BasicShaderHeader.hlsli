// t# テクスチャバッファ
// b# 定数バッファ
// s# サンプラーのレジスタ
// https://learn.microsoft.com/ja-jp/windows/win32/direct3dgetstarted/work-with-shaders-and-shader-resources
Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

cbuffer cbuff0 : register(b0)
{
    matrix world;
    matrix viewproj;
};

//定数バッファー
//マテリアル用
cbuffer Material : register(b1)
{
    float4 diffuse;
    float4 specular;
    float3 ambient;
}

struct Output
{
    float4 svpos : SV_POSITION; // 頂点座標
    float4 normal : NORMAL; // 法線ベクトル
    float2 uv : TEXCOORD; // uv値
};