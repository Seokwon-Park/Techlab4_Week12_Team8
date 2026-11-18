#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleRotation.generated.h"

// UE: UParticleModuleRotation (Cascade 이름: Initial Rotation)
// 태어날 때 스프라이트의 회전 각도를 정한다. 단위는 UE처럼 "바퀴" (1 = 360도)
UCLASS()
class UParticleModuleRotation : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleRotation();
	virtual ~UParticleModuleRotation() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	// 기본: 0 ~ 1바퀴 랜덤 (파티클마다 다른 방향)
	UPROPERTY(EditAnywhere)
	FRawDistributionFloat StartRotation = FRawDistributionFloat::MakeUniform(0.0f, 1.0f);
};
