#include "EnginePCH.h"
#include "ParticleModuleRotationRateMultiplyLife.h"
#include "Particles/ParticleEmitterInstances.h"

UParticleModuleRotationRateMultiplyLife::UParticleModuleRotationRateMultiplyLife()
{
	bUpdateModule = true;
}

void UParticleModuleRotationRateMultiplyLife::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	// 매 프레임 RotationRate는 BaseRotationRate로 돌아와 있으므로, 곱해도 누적되지 않는다 (SizeByLife와 같은 방식)
	BEGIN_UPDATE_LOOP;
	Particle.RotationRate *= LifeMultiplier.GetValue(Particle.RelativeTime);
	END_UPDATE_LOOP;
}
