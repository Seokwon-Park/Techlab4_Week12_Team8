#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleSpawn.generated.h"

UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleSpawn() = default;
	virtual ~UParticleModuleSpawn() override = default;

	UPROPERTY(EditAnywhere)
	float Rate = 20.0f;
};