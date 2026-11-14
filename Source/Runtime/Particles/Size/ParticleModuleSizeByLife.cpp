#include "EnginePCH.h"
#include "ParticleModuleSizeByLife.h"
#include "Particles/ParticleEmitterInstances.h"

UParticleModuleSizeByLife::UParticleModuleSizeByLife()
{
	bUpdateModule = true;
}

void UParticleModuleSizeByLife::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	BEGIN_UPDATE_LOOP;
	Particle.Size *= LifeMultiplier.GetValue(Particle.RelativeTime);
	END_UPDATE_LOOP;
}