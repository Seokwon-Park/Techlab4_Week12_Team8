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
	virtual ~UParticleEmitter() override;

	UPROPERTY(EditAnywhere)
	FString EmitterName = "Particle Emitter";

	using Super::Serialize;
	void Serialize(FStructuredArchive::FRecord Record) override;

	// 가까운 순서로 저장한다. 실행 전에 AddLODLevel()로 구성한다.
	TArray<UParticleLODLevel*> LODLevels;

	int32 PeakActiveParticles = 0; // 최대 파티클 수

	/** Initial allocation count - overrides calculated peak count if > 0 */
	int32 InitialAllocationCount = 0; //초기 파티클 수

	//////////////////////////////////////////////////////////////////////////
	// Below is information udpated by calling CacheEmitterModuleInfo

	/** Map module pointers to their offset into the particle data. 페이로드가 있는 모듈만 들어 있다 */
	TMap<UParticleModule*, uint32> ModuleOffsetMap; // 파티클별 데이터 안에서 모듈 데이터의 위치

	/** Map module pointers to their offset into the instance data. */
	TMap<UParticleModule*, uint32> ModuleInstanceOffsetMap; // 인스턴스별 데이터 안에서 모듈 데이터의 위치

	/** FBaseParticle + 모든 페이로드. *16바이트 올림 전 값이다* -> Padding 제외한 값 */
	int32 ParticleSize = sizeof(FBaseParticle);
	/* TypeDataInstance + ModuleInstances */
	int32 ReqInstanceBytes = 0;

	int32 TypeDataOffset = 0; // TypeData 모듈의 페이로드 offset. 없으면 0
	int32 TypeDataInstanceOffset = -1;

	//////////////////////////////////////////////////////////////////////////

	/** 범위를 벗어나면 nullptr */
	UParticleLODLevel* GetLODLevel(int32 LODLevel) const;

	/** LOD0은 거리 0, 이후는 증가하는 거리로 등록한다. 잘못된 구성은 -1을 반환한다. */
	int32 AddLODLevel(UParticleLODLevel* LODLevel, float MinDistance);

	/** 거리 이하의 가장 먼 LOD를 선택한다. 유효한 LOD0이 없으면 -1. */
	int32 SelectLODLevel(float Distance) const;

	/** LOD 간 모듈 클래스/순서, TypeData 종류, 로컬 공간 설정은 같아야 한다. */
	bool IsLODLevelCompatible(const UParticleLODLevel* LODLevel) const;

	/**
	 * Builds data needed for simulation by the emitter from all modules.
	 * LOD0의 모듈 순서로 공통 배치를 만들고, 각 슬롯은 모든 LOD의 최대 크기를 확보한다.
	 * 같은 슬롯의 LOD별 모듈 포인터는 같은 offset으로 매핑한다. 실행 중 구성 변경은 재초기화가 필요하다.
	 */
	void CacheEmitterModuleInfo();
};
