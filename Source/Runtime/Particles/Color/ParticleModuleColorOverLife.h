#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleColorOverLife.generated.h"

UCLASS()
class UParticleModuleColorOverLife : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleColorOverLife();
	virtual ~UParticleModuleColorOverLife() override = default;

	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;

	UPROPERTY(EditAnywhere)
	FRawDistributionVector ColorOverLife = FRawDistributionVector::MakeConstant(FVector(1.0f, 1.0f, 1.0f));

	UPROPERTY(EditAnywhere)
	FRawDistributionFloat AlphaOverLife = FRawDistributionFloat::MakeConstant(1.0f);
};