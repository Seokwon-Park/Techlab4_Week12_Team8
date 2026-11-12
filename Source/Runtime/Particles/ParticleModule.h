#pragma once

// ============================================================================
// [Week12 0단계 공통 계약] UE의 Classes/Particles/ParticleModule.h 에 대응한다.
//
// 구체 모듈(Spawn, Lifetime, Location, Velocity, Color, Size ...)은 모듈 담당이
// UE처럼 Particles/<분류>/ParticleModule<이름>.h 로 추가해 UParticleModule을 상속해서 만든다.
// ============================================================================

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Particles/ParticleHelper.h"
#include "ParticleModule.generated.h"

class UParticleModuleTypeDataBase;
struct FParticleEmitterInstance;

UCLASS()
class UParticleModule : public UObject
{
	GENERATED_BODY()

public:
	UParticleModule()
		: bSpawnModule(false)
		, bUpdateModule(false)
		, bFinalUpdateModule(false)
	{
	}
	virtual ~UParticleModule() override = default;

	/** If true, the module performs operations on particles during Spawning. 파생 모듈의 생성자에서 켠다 */
	uint8 bSpawnModule:1;

	/** If true, the module performs operations on particles during Updating. */
	uint8 bUpdateModule:1;

	/** If true, the module performs operations on particles during final update. */
	uint8 bFinalUpdateModule:1;

	/** If true, the module is enabled. (UE는 uint8 bEnabled:1. 이 엔진의 리플렉션에 맞춰 bool로 둔다) */
	UPROPERTY(EditAnywhere)
	bool bEnabled = true;

	/**
	 *	Called on a particle that is freshly spawned by the emitter.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that spawned the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	SpawnTime	The time of the spawn.
	 *	@param	ParticleBase	방금 생성된 파티클. 본문 첫 줄에 SPAWN_INIT을 쓰면 Particle로 접근할 수 있다.
	 */
	virtual void	Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) {}

	/**
	 *	Called on a particle that is being updated by its emitter.
	 *
	 *	@param	Owner		The FParticleEmitterInstance that 'owns' the particle.
	 *	@param	Offset		The modules offset into the data payload of the particle.
	 *	@param	DeltaTime	The time since the last update.
	 */
	virtual void	Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) {}

	/**
	 *	Called on an emitter when all other update operations have taken place
	 *	INCLUDING bounding box cacluations!
	 */
	virtual void	FinalUpdate(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) {}

	/**
	 *	Returns the number of bytes that the module requires in the particle payload block.
	 *
	 *	@param	TypeData	The UParticleModuleTypeDataBase for the emitter that contains this module.
	 *	@return	uint32		The number of bytes the module needs per particle.
	 */
	virtual uint32	RequiredBytes(UParticleModuleTypeDataBase* TypeData) { return 0; }

	/**
	 *	Returns the number of bytes the module requires in the emitters 'per-instance' data block.
	 *
	 *	@return uint32		The number of bytes the module needs per emitter instance.
	 */
	virtual uint32	RequiredBytesPerInstance() { return 0; }
};
