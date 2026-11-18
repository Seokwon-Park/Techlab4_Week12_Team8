#include "EnginePCH.h"
#include "ParticleModuleRotationRate.h"
#include "Particles/ParticleEmitterInstances.h"

namespace
{
	constexpr float TwoPi = 6.28318530718f;
}

UParticleModuleRotationRate::UParticleModuleRotationRate()
{
	bSpawnModule = true;
}

void UParticleModuleRotationRate::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;
	const float Rate = StartRotationRate.GetValue(Owner->EmitterTime) * TwoPi;
	// 코어가 매 프레임 RotationRate = BaseRotationRate로 되돌리므로 Base에 넣어야 유지된다
	Particle.BaseRotationRate += Rate;
	Particle.RotationRate += Rate;
}
