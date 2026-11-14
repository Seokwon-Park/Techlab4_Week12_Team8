#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleSizeByLife.generated.h"

UCLASS()
class UParticleModuleSizeByLife : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleSizeByLife();
	virtual ~UParticleModuleSizeByLife() override = default;

	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;

	UPROPERTY(EditAnywhere)
	FRawDistributionVector LifeMultiplier = FRawDistributionVector::MakeConstant(FVector(1.0f, 1.0f, 1.0f));
};