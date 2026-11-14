#include "EnginePCH.h"
#include "ParticleModuleLocation.h"
#include "Particles/ParticleEmitterInstances.h"

UParticleModuleLocation::UParticleModuleLocation()
{
	bSpawnModule = true;
}

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;
	Particle.Location += StartLocation.GetValue(Owner->EmitterTime);
}