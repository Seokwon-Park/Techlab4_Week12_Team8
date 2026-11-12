#pragma once

// ============================================================================
// [Week12 0단계 공통 계약] UE의 Classes/Particles/TypeData/ParticleModuleTypeDataBase.h 에 대응한다.
//
// Emitter 종류를 정하는 모듈이다. LODLevel의 TypeDataModule이 nullptr이면 Sprite Emitter다.
// Mesh / Beam / Ribbon은 이 폴더에 ParticleModuleTypeData<이름>.h 로 추가한다.
// ============================================================================

#include "Particles/ParticleModule.h"
#include "ParticleModuleTypeDataBase.generated.h"

class UParticleEmitter;
class UParticleSystemComponent;

UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleTypeDataBase() = default;

	// 이 타입에 맞는 FParticleEmitterInstance 파생 객체를 new로 만들어 돌려준다. nullptr이면 Sprite 인스턴스를 쓴다.
	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent) { return nullptr; }

	virtual bool		IsAMeshEmitter() const	{ return false; }
};
