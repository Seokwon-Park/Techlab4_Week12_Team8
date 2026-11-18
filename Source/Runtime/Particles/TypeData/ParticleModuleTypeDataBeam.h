#pragma once

#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "ParticleModuleTypeDataBeam.generated.h"

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
};