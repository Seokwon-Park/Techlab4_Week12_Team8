#ifndef MESH_PARTICLE_INSTANCING_HLSLI
#define MESH_PARTICLE_INSTANCING_HLSLI

#if MESH_PARTICLE_INSTANCING
#define MESH_PARTICLE_VS_INPUT \
    float4 InstanceX : INSTANCE_TRANSFORM0; \
    float4 InstanceY : INSTANCE_TRANSFORM1; \
    float4 InstanceZ : INSTANCE_TRANSFORM2;
#define MESH_PARTICLE_WORLD(Input) GetMeshParticleWorld(Input.InstanceX, Input.InstanceY, Input.InstanceZ)
#define MESH_PARTICLE_NORMAL(World) GetMeshParticleNormalMatrix(World)

float4x4 GetMeshParticleWorld(float4 X, float4 Y, float4 Z)
{
    return transpose(float4x4(X, Y, Z, float4(0.0, 0.0, 0.0, 1.0)));
}

float3x3 GetMeshParticleNormalMatrix(float4x4 WorldTransform)
{
    float3 X = WorldTransform[0].xyz;
    float3 Y = WorldTransform[1].xyz;
    float3 Z = WorldTransform[2].xyz;
    float3 CofactorX = cross(Y, Z);
    float Determinant = dot(X, CofactorX);
    if (abs(Determinant) <= 1.0e-20)
        return float3x3(1, 0, 0, 0, 1, 0, 0, 0, 1);
    return float3x3(CofactorX, cross(Z, X), cross(X, Y)) / Determinant;
}
#else
#define MESH_PARTICLE_VS_INPUT
#define MESH_PARTICLE_WORLD(Input) World
#define MESH_PARTICLE_NORMAL(World) ((float3x3)NormalMatrix)
#endif

#endif
