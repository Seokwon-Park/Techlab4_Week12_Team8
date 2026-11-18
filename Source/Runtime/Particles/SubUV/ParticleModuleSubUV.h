#pragma once

// ============================================================================
// UE의 UParticleModuleSubUV (Cascade 이름: SubImage Index)에 대응한다.
//
// 스프라이트 시트(Required의 SubImagesHorizontal × SubImagesVertical 칸)에서
// 파티클마다 몇 번째 칸을 그릴지 정한다. 값은 파티클 페이로드의 float 하나에 두고,
// 이미터 인스턴스가 그 위치를 SubUVDataOffset으로 렌더러(SceneProxy)에 알려 준다.
// ============================================================================

#include "Particles/ParticleModule.h"
#include "ParticleModuleSubUV.generated.h"

UENUM()
enum class ESubUVMode : uint8
{
	OverLife,       // 태어날 때 첫 칸 → 죽을 때 마지막 칸 (UE: Linear). 수명이 달라도 시트를 한 번 다 돈다
	FrameRate,      // 초당 FramesPerSecond 칸씩 넘기고 끝나면 처음으로 (불꽃·연기 루프)
	Random,         // 태어날 때 칸 하나를 골라 고정 (모양만 다른 파편)
};

// 파티클 하나에 붙는 데이터. ImageIndex가 맨 앞이어야 한다 (SceneProxy가 SubUVDataOffset에서 float 하나를 읽는다)
struct FSubUVPayload
{
	float ImageIndex;   // 지금 그릴 칸 번호 (0 ~ 칸 수 - 1)
	float StartFrame;   // 시작 칸 (bRandomStartFrame이면 랜덤)
};

UCLASS()
class UParticleModuleSubUV : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleSubUV();
	virtual ~UParticleModuleSubUV() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override { return sizeof(FSubUVPayload); }

	UPROPERTY(EditAnywhere)
	ESubUVMode Mode = ESubUVMode::OverLife;

	// FrameRate 모드에서 1초에 넘길 칸 수
	UPROPERTY(EditAnywhere)
	float FramesPerSecond = 30.0f;

	// 파티클마다 시작 칸을 랜덤으로 (같은 애니메이션이 한꺼번에 똑같이 움직이지 않게)
	UPROPERTY(EditAnywhere)
	bool bRandomStartFrame = false;

private:
	static int32 GetFrameCount(const FParticleEmitterInstance* Owner);
	float CalcImageIndex(const FBaseParticle& Particle, float StartFrame, int32 FrameCount) const;
};
