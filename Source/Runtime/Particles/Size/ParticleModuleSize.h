#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleSize.generated.h"

UCLASS()
class UParticleModuleSize : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleSize();
	virtual ~UParticleModuleSize() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	UPROPERTY(EditAnywhere)
	FRawDistributionVector StartSize = FRawDistributionVector::MakeConstant(FVector(1.0f, 1.0f, 1.0f));
};