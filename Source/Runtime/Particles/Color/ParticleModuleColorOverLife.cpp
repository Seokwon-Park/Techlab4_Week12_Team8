#include "EnginePCH.h"
#include "ParticleModuleColorOverLife.h"
#include "Particles/ParticleEmitterInstances.h"

UParticleModuleColorOverLife::UParticleModuleColorOverLife()
{
	bUpdateModule = true;
}

void UParticleModuleColorOverLife::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	BEGIN_UPDATE_LOOP;
	const FVector ColorVec = ColorOverLife.GetValue(Particle.RelativeTime);
	const float Alpha = AlphaOverLife.GetValue(Particle.RelativeTime);
	Particle.Color = FLinearColor(ColorVec.X, ColorVec.Y, ColorVec.Z, Alpha);
	END_UPDATE_LOOP;
}