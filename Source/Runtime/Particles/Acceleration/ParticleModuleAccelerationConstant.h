#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleAccelerationConstant.generated.h"

// UE: UParticleModuleAccelerationConstant (Cascade 이름: Const Acceleration)
// 모든 파티클에 같은 가속도를 준다. 중력, 바람, 위로 솟는 연기 등
UCLASS()
class UParticleModuleAccelerationConstant : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleAccelerationConstant();
	virtual ~UParticleModuleAccelerationConstant() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;

	// 초당 속도 변화량 (단위/초²). 예: (0, 0, -9.8)이면 아래로 떨어진다
	UPROPERTY(EditAnywhere)
	FVector Acceleration = FVector(0.0f, 0.0f, 0.0f);
};
