#pragma pack_matrix(row_major)

// FRenderer::UploadViewMatrix가 b0에 ViewProjection 하나만 올린다
cbuffer ViewConstants : register(b0)
{
    matrix VP;
};

// FParticleSpriteConstants (Packet.MaterialParamData). C++ 구조체와 바이트 단위로 같아야 한다
cbuffer SpriteConstants : register(b1)
{
    float4 CameraRight; // xyz
    float4 CameraUp; // xyz
    float4 SubUVInfo; // x: 가로 칸, y: 세로 칸, z: 1이면 Additive
};

// 반투명 패스의 FPerObjectConstants. 첫 멤버가 World (월드 공간 이미터는 단위 행렬)
cbuffer ObjectConstants : register(b2)
{
    matrix World;
};

Texture2D SpriteTexture : register(t0);
SamplerState SpriteSampler : register(s0);

// FParticleSpriteVertex와 같은 순서, 같은 크기 (64바이트)
struct VS_INPUT
{
    float3 Position : POSITION;
    float RelativeTime : RELATIVETIME;
    float3 OldPosition : OLDPOSITION;
    float ParticleId : PARTICLEID;
    float2 Size : SIZE;
    float Rotation : ROTATION;
    float SubImageIndex : SUBIMAGEINDEX;
    float4 Color : COLOR;
    uint VertexId : SV_VertexID;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
    float RelativeTime : TEXCOORD1; // 지금은 안 쓰지만 넘겨 두면 입력이 최적화로 빠지지 않는다
};

// 인덱스 버퍼의 0,1,2 / 0,2,3 순서와 맞춘다
static const float2 Corners[4] =
{
    float2(-0.5f, -0.5f), // 0: 왼쪽 아래
    float2(0.5f, -0.5f), // 1: 오른쪽 아래
    float2(0.5f, 0.5f), // 2: 오른쪽 위
    float2(-0.5f, 0.5f), // 3: 왼쪽 위
};

PS_INPUT mainVS(VS_INPUT In)
{
    PS_INPUT Out;
    const float2 Corner = Corners[In.VertexId & 3];

    // 화면 평면에서 회전한 뒤 크기를 곱한다
    float S, C;
    sincos(In.Rotation, S, C);
    const float2 Offset = float2(C * Corner.x - S * Corner.y, S * Corner.x + C * Corner.y) * In.Size;

    const float3 Center = mul(float4(In.Position, 1.0f), World).xyz;
    const float3 WorldPos = Center + CameraRight.xyz * Offset.x + CameraUp.xyz * Offset.y;
    Out.Position = mul(float4(WorldPos, 1.0f), VP);

    // SubUV: 칸 번호 → (열, 행) → 칸 안의 UV
    const float2 Cells = max(SubUVInfo.xy, 1.0f);
    const float Frame = floor(In.SubImageIndex);
    const float2 Cell = float2(fmod(Frame, Cells.x), floor(Frame / Cells.x));
    const float2 LocalUV = float2(Corner.x + 0.5f, 0.5f - Corner.y); // 회전 전 모서리 기준
    Out.UV = (Cell + LocalUV) / Cells;

    Out.Color = In.Color;
    Out.RelativeTime = In.RelativeTime;
    return Out;
}

float4 mainPS(PS_INPUT In) : SV_TARGET
{
    float4 Color = SpriteTexture.Sample(SpriteSampler, In.UV) * In.Color;

    // Additive 블렌드 상태는 ONE / ONE이라 알파가 반영되지 않는다 → rgb에 미리 곱한다
    if (SubUVInfo.z > 0.5f)
    {
        Color.rgb *= Color.a;
    }
    return Color;
}