#include "EnginePCH.h"
#include "ParticleModuleLifetime.h"
#include "Particles/ParticleEmitterInstances.h"

UParticleModuleLifetime::UParticleModuleLifetime()
{
	bSpawnModule = true;
}

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;
	const float MaxLifetime = Lifetime.GetValue(Owner->EmitterTime);

	if (MaxLifetime <= 0.0f)
	{
		// MaxLifetime이 0 이하일 경우 즉시 죽음
		Particle.RelativeTime = 1.1f;
	}
	else
	{
		Particle.OneOverMaxLifetime = 1.0f / MaxLifetime;
	}
}