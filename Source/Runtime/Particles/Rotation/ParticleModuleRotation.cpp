#include "EnginePCH.h"
#include "ParticleModuleRotation.h"
#include "Particles/ParticleEmitterInstances.h"

namespace
{
	constexpr float TwoPi = 6.28318530718f;   // 바퀴 → 라디안
}

UParticleModuleRotation::UParticleModuleRotation()
{
	bSpawnModule = true;
}

void UParticleModuleRotation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;
	// FBaseParticle.Rotation은 라디안. 셰이더가 이 각도로 쿼드를 돌린다
	Particle.Rotation += StartRotation.GetValue(Owner->EmitterTime) * TwoPi;
}
