#include "EnginePCH.h"
#include "ParticleModuleVelocity.h"
#include "Particles/ParticleEmitterInstances.h"

UParticleModuleVelocity::UParticleModuleVelocity()
{
	bSpawnModule = true;
}

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;
	const FVector Velocity = StartVelocity.GetValue(Owner->EmitterTime);
	Particle.BaseVelocity = Velocity;
	Particle.Velocity = Velocity;
}