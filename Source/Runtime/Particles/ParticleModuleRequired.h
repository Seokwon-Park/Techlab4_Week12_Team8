#pragma once

// ============================================================================
// [Week12 0단계 공통 계약] UE의 Classes/Particles/ParticleModuleRequired.h 에 대응한다.
// 필드는 담당자가 UE 원본을 참고해 필요한 만큼 추가한다.
// ============================================================================

#include "Particles/ParticleModule.h"
#include "Render/MaterialInterface.h"
#include "ParticleModuleRequired.generated.h"

enum EParticleSortMode : int
{
	PSORTMODE_None,
	PSORTMODE_ViewProjDepth,
	PSORTMODE_DistanceToView,
	PSORTMODE_Age_OldestFirst,
	PSORTMODE_Age_NewestFirst,
	PSORTMODE_MAX,
};

/** Required 모듈에서 렌더러가 쓰는 값만 뽑은 사본. FDynamicSpriteEmitterReplayDataBase::RequiredModule이 가리킨다. */
struct FParticleRequiredModule
{
	uint32 NumFrames;
	uint32 NumBoundingVertices;
	uint32 NumBoundingTriangles;
	float AlphaThreshold;
	TArray<FVector2> FrameData;
	uint8 bCutoutTexureIsValid : 1;
	uint8 bUseVelocityForMotionBlur : 1;
};

UCLASS()
class UParticleModuleRequired : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleRequired() = default;

	/** The material to utilize for the emitter at this LOD level. */
	UPROPERTY(EditAnywhere)
	UMaterialInterface* Material = nullptr;

	/** If true, update the emitter in local space */
	UPROPERTY(EditAnywhere)
	bool bUseLocalSpace = false;

	/** The sorting mode to use for this emitter. EParticleSortMode */
	UPROPERTY(EditAnywhere)
	int32 SortMode = PSORTMODE_None;
};
