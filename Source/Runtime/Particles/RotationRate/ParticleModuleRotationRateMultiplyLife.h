#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleRotationRateMultiplyLife.generated.h"

// UE: UParticleModuleRotationRateMultiplyLife (Cascade 이름: RotationRate * Life)
// 수명 비율(0 → 1)에 따라 회전 속도에 배율을 곱한다. 예: ConstantCurve 1 → 0이면 점점 느려져 멈춘다
UCLASS()
class UParticleModuleRotationRateMultiplyLife : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleRotationRateMultiplyLife();
	virtual ~UParticleModuleRotationRateMultiplyLife() override = default;

	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;

	UPROPERTY(EditAnywhere)
	FRawDistributionFloat LifeMultiplier = FRawDistributionFloat::MakeConstantCurve(1.0f, 0.0f);
};
