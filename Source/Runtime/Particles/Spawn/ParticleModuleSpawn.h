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
	virtual ~UParticleModuleSpawn() override = default;

	UPROPERTY(EditAnywhere)
	float Rate = 20.0f;
};