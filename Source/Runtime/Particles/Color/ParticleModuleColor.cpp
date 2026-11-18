#include "EnginePCH.h"
#include "ParticleModuleColor.h"
#include "Particles/ParticleEmitterInstances.h"

UParticleModuleColor::UParticleModuleColor()
{
	bSpawnModule = true;
}

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;
	const FVector ColorVec = StartColor.GetValue(Owner->EmitterTime);
	const float Alpha = StartAlpha.GetValue(Owner->EmitterTime);
	const FLinearColor NewColor(ColorVec.X, ColorVec.Y, ColorVec.Z, Alpha);
	Particle.BaseColor = NewColor;
	Particle.Color = NewColor;
}