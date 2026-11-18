#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleLifetime.generated.h"

UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleLifetime();
	virtual ~UParticleModuleLifetime() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	UPROPERTY(EditAnywhere)
	FRawDistributionFloat Lifetime = FRawDistributionFloat::MakeConstant(1.0f);
};