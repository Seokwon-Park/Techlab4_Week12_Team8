#include "EnginePCH.h"
#include "ParticleModuleSize.h"
#include "Particles/ParticleEmitterInstances.h"

UParticleModuleSize::UParticleModuleSize()
{
	bSpawnModule = true;
}

void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;
	const FVector Size = StartSize.GetValue(Owner->EmitterTime);
	Particle.BaseSize = Size;
	Particle.Size = Size;
}