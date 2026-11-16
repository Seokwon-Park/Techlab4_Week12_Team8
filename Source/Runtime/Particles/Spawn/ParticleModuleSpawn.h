#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleSpawn.generated.h"

// 생성 개수는 이미터 코어가 결정한다. 이 모듈은 초당 생성률 설정을 제공한다.
UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleSpawn() = default;

	/** Number of particles to spawn per second. 0 이하이면 생성하지 않는다. */
	UPROPERTY(EditAnywhere)
	float Rate = 10.0f;
};
