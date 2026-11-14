#include "EnginePCH.h"
#include "ParticleModuleSizeScaleBySpeed.h"
#include "Particles/ParticleEmitterInstances.h"

UParticleModuleSizeScaleBySpeed::UParticleModuleSizeScaleBySpeed()
{
	bUpdateModule = true;
}

void UParticleModuleSizeScaleBySpeed::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	FVector Scale(SpeedScale.X, SpeedScale.Y, 1.0f);
	FVector ScaleMax(MaxScale.X, MaxScale.Y, 1.0f);
	BEGIN_UPDATE_LOOP;
	FVector Size = Scale * Particle.Velocity.Size();
	Size = Size.ComponentMax(FVector(1.0f));
	Size = Size.ComponentMin(ScaleMax);
	Particle.Size = Particle.BaseSize * Size;
	END_UPDATE_LOOP;
}