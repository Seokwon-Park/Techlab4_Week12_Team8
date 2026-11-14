#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleSizeScaleBySpeed.generated.h"

UCLASS()
class UParticleModuleSizeScaleBySpeed : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleSizeScaleBySpeed();
	virtual ~UParticleModuleSizeScaleBySpeed() override = default;

	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;

	UPROPERTY(EditAnywhere)
	FVector2 SpeedScale;

	UPROPERTY(EditAnywhere)
	FVector2 MaxScale = FVector2(1.0f);
};