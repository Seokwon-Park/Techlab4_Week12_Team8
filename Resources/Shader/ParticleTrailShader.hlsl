#pragma pack_matrix(row_major)

cbuffer ViewConstants : register(b0)
{
    matrix VP;
};

// FParticleSpriteConstants를 그대로 쓴다. 띠는 SubUVInfo.z(Additive 여부)만 읽는다
cbuffer SpriteConstants : register(b1)
{
    float4 CameraRight;
    float4 CameraUp;
    float4 SubUVInfo;
};

// 띠 정점은 이미 월드 좌표라 World는 단위 행렬이다
cbuffer ObjectConstants : register(b2)
{
    matrix World;
};

Texture2D TrailTexture : register(t0);
SamplerState TrailSampler : register(s0);

// FParticleTrailVertex와 같은 순서 (36바이트)
struct VS_INPUT
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
};

PS_INPUT mainVS(VS_INPUT In)
{
    PS_INPUT Out;
    Out.Position = mul(mul(float4(In.Position, 1.0f), World), VP);
    Out.UV = In.UV;
    Out.Color = In.Color;
    return Out;
}

float4 mainPS(PS_INPUT In) : SV_TARGET
{
    float4 Color = TrailTexture.Sample(TrailSampler, In.UV) * In.Color;
    if (SubUVInfo.z > 0.5f)   // Additive: ONE/ONE라 알파를 미리 곱한다
    {
        Color.rgb *= Color.a;
    }
    return Color;
}