#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleAccelerationDrag.generated.h"

// UE: UParticleModuleAccelerationDrag (Cascade 이름: Drag)
// 공기 저항. 매초 속도의 DragCoefficient 비율만큼 줄인다. 퍼졌다가 멈추는 폭발 파편, 느려지는 연기
UCLASS()
class UParticleModuleAccelerationDrag : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleAccelerationDrag();
	virtual ~UParticleModuleAccelerationDrag() override = default;

	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;

	// 수명 비율(0 → 1)로 계산. 1이면 1초에 속도가 대략 37%(e^-1)로 줄어든다
	UPROPERTY(EditAnywhere)
	FRawDistributionFloat DragCoefficient = FRawDistributionFloat::MakeConstant(1.0f);
};
