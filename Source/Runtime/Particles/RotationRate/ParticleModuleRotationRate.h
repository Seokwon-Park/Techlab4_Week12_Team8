#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleRotationRate.generated.h"

// UE: UParticleModuleRotationRate (Cascade 이름: Initial RotationRate)
// 태어날 때 회전 속도를 정한다. 단위는 초당 바퀴 (1 = 1초에 한 바퀴). 회전 적분은 코어가 한다
UCLASS()
class UParticleModuleRotationRate : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleRotationRate();
	virtual ~UParticleModuleRotationRate() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	// 기본: -0.5 ~ 0.5 바퀴/초 랜덤 (양쪽 방향으로 천천히)
	UPROPERTY(EditAnywhere)
	FRawDistributionFloat StartRotationRate = FRawDistributionFloat::MakeUniform(-0.5f, 0.5f);
};
