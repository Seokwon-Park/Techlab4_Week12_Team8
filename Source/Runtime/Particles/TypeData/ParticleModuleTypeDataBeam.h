#pragma once

#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "ParticleModuleTypeDataBeam.generated.h"

// 빔을 흔드는 방식
UENUM()
enum class EBeamNoiseType : uint8
{
	Sine,        // 부드럽게 출렁이는 띠 (에너지 빔, 밧줄)
	Lightning,   // 지그재그. NoiseRefreshRate마다 모양을 새로 뽑아 번쩍인다 (번개, 전기)
};

UCLASS()
class UParticleModuleTypeDataBeam : public UParticleModuleTypeDataBase
{
	GENERATED_BODY()

public:
	UParticleModuleTypeDataBeam() = default;

	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent) override;

	UPROPERTY(EditAnywhere)
	FVector TargetOffset = FVector(5.0f, 0.0f, 0.0f);
	UPROPERTY(EditAnywhere)
	int32 Segments = 16;
	UPROPERTY(EditAnywhere)
	float NoiseAmplitude = 0.3f;
	UPROPERTY(EditAnywhere)
	float NoiseFrequency = 3.0f;
	UPROPERTY(EditAnywhere)
	float NoiseSpeed = 10.0f;
	UPROPERTY(EditAnywhere)
	float TextureTile = 1.0f;
	UPROPERTY(EditAnywhere)
	EBeamNoiseType NoiseType = EBeamNoiseType::Sine;
	// Lightning: 1초에 모양을 몇 번 새로 뽑을지
	UPROPERTY(EditAnywhere)
	float NoiseRefreshRate = 20.0f;
};