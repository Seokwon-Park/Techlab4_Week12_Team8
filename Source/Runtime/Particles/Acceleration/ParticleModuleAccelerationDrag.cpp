#include "EnginePCH.h"
#include "ParticleModuleAccelerationDrag.h"
#include "Particles/ParticleEmitterInstances.h"

#include <algorithm>

UParticleModuleAccelerationDrag::UParticleModuleAccelerationDrag()
{
	bUpdateModule = true;
}

void UParticleModuleAccelerationDrag::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	BEGIN_UPDATE_LOOP;
	// UE와 같은 식: v -= v * Drag * dt. 한 프레임에 속도가 뒤집히지 않게 비율을 1로 자른다 (긴 프레임 대비)
	const float Factor = std::clamp(DragCoefficient.GetValue(Particle.RelativeTime) * DeltaTime, 0.0f, 1.0f);
	Particle.BaseVelocity -= Particle.BaseVelocity * Factor;   // Base에도 적용해야 다음 프레임까지 유지된다
	Particle.Velocity -= Particle.Velocity * Factor;
	END_UPDATE_LOOP;
}
