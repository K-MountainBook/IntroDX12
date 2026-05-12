
Texture2D<float4> tex   : register(t0);
SamplerState smp        : register(s0);

cbuffer cbuff0 : register(b0)
{
    matrix mat;
};

struct Output
{
    float4 svpos : SV_POSITION; // í∏ì_ç¿ïW
    float2 uv    : TEXCOORD;    // uvíl
};