#pragma once

#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Render/Mesh.h"
#include "ParticleModuleTypeDataMesh.generated.h"

UCLASS()
class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	UStaticMesh* StaticMesh = nullptr;

	FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent,
		UParticleSystemComponent* InComponent) override;
	bool IsAMeshEmitter() const override { return true; }
};
