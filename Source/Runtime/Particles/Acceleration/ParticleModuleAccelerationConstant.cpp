#include "EnginePCH.h"
#include "ParticleModuleAccelerationConstant.h"
#include "Particles/ParticleEmitterInstances.h"

UParticleModuleAccelerationConstant::UParticleModuleAccelerationConstant()
{
	bSpawnModule = true;
	bUpdateModule = true;
}

void UParticleModuleAccelerationConstant::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;
	// 프레임 중간에 태어난 파티클은 남은 시간(SpawnTime)만큼 이미 가속된 상태로 시작한다 (UE와 같음)
	const FVector Delta = Acceleration * SpawnTime;
	Particle.BaseVelocity += Delta;
	Particle.Velocity += Delta;
}

void UParticleModuleAccelerationConstant::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	const FVector Delta = Acceleration * DeltaTime;
	BEGIN_UPDATE_LOOP;
	// 코어가 매 프레임 Velocity = BaseVelocity로 되돌리므로, 속도가 쌓이려면 Base에도 더해야 한다
	Particle.BaseVelocity += Delta;
	Particle.Velocity += Delta;
	END_UPDATE_LOOP;
}
