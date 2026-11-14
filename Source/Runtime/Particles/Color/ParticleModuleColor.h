#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleColor.generated.h"

UCLASS()
class UParticleModuleColor : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleColor();
	virtual ~UParticleModuleColor() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	UPROPERTY(EditAnywhere)
	FRawDistributionVector StartColor = FRawDistributionVector::MakeConstant(FVector(1.0f, 1.0f, 1.0f));

	UPROPERTY(EditAnywhere)
	FRawDistributionFloat StartAlpha = FRawDistributionFloat::MakeConstant(1.0f);
};