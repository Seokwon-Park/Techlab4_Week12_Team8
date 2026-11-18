#pragma once

#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "ParticleModuleTypeDataRibbon.generated.h"

UCLASS()
class UParticleModuleTypeDataRibbon : public UParticleModuleTypeDataBase
{
	GENERATED_BODY()

public:
	UParticleModuleTypeDataRibbon() = default;

	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent) override;

	UPROPERTY(EditAnywhere)
	float TilingDistance = 0.0f;
	UPROPERTY(EditAnywhere)
	bool bConnectToSource = true;
};