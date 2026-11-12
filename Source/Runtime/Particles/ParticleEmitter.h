#pragma once

// ============================================================================
// [Week12 0단계 공통 계약] UE의 Classes/Particles/ParticleEmitter.h 에 대응한다.
// ============================================================================

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Particles/ParticleHelper.h"
#include "ParticleEmitter.generated.h"

class UParticleLODLevel;
class UParticleModule;

UCLASS()
class UParticleEmitter : public UObject
{
	GENERATED_BODY()

public:
	UParticleEmitter() = default;

	TArray<UParticleLODLevel*> LODLevels;

	int32 PeakActiveParticles = 0;

	/** Initial allocation count - overrides calculated peak count if > 0 */
	int32 InitialAllocationCount = 0;

	//////////////////////////////////////////////////////////////////////////
	// Below is information udpated by calling CacheEmitterModuleInfo

	/** Map module pointers to their offset into the particle data. 페이로드가 있는 모듈만 들어 있다 */
	TMap<UParticleModule*, uint32> ModuleOffsetMap;

	/** Map module pointers to their offset into the instance data. */
	TMap<UParticleModule*, uint32> ModuleInstanceOffsetMap;

	/** FBaseParticle + 모든 페이로드. 16바이트 올림 전 값이다 */
	int32 ParticleSize = sizeof(FBaseParticle);
	int32 ReqInstanceBytes = 0;

	int32 TypeDataOffset = 0;
	int32 TypeDataInstanceOffset = -1;

	//////////////////////////////////////////////////////////////////////////

	/** 범위를 벗어나면 nullptr */
	UParticleLODLevel* GetLODLevel(int32 LODLevel);

	/**
	 * Builds data needed for simulation by the emitter from all modules.
	 * 페이로드 offset 규칙을 구현하는 자리다. 모듈 구성이 바뀔 때마다 다시 호출한다. offset은 LOD 0의 모듈 순서로 정한다.
	 */
	void CacheEmitterModuleInfo();
};
