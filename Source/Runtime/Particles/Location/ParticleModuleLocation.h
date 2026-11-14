#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleLocation.generated.h"

UCLASS()
class UParticleModuleLocation : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleLocation();
	virtual ~UParticleModuleLocation() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	UPROPERTY(EditAnywhere)
	FRawDistributionVector StartLocation;
};