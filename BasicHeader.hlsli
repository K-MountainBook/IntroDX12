struct Output
{
    float4 svpos : SV_POSITION; // システム用頂点座標
    float4 normal : NORMAL; // 法線ベクトル
    float4 vnormal : NORMAL1; 
    float2 uv : TEXCOORD; // uv座標
};

Texture2D<float4> tex : register(t0); // テクスチャレジスタ0番
Texture2D<float4> sph : register(t1); // 1番スロットに設定されたテクスチャ
Texture2D<float4> spa : register(t2);
SamplerState smp : register(s0); // サンプラーレジスタ0番

cbuffer cbuff0 : register(b0)
{
    matrix world;
    matrix viewproj;
    matrix view;
    matrix proj;
};

cbuffer Material : register(b1)
{
    float4 diffuse;
    float4 specular;
    float3 ambient;
}