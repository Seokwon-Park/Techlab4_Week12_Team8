#pragma once

// ============================================================================
// UE의 UParticleModuleLight (Cascade 이름: Light)에 대응한다.
//
// 파티클마다 점광원을 하나씩 단다. 값은 파티클 페이로드(FLightParticlePayload)에 두고,
// 렌더러(FSceneRenderer::InitViews)가 ReplayData.LightDataOffset으로 읽어 레벨 광원과 같은 목록에 넣는다.
// 그림자는 만들지 않는다 (UE 파티클 라이트도 기본은 그림자 없음).
// ============================================================================

#include "Particles/ParticleModule.h"
#include "ParticleModuleLight.generated.h"

UCLASS()
class UParticleModuleLight : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleLight();
	virtual ~UParticleModuleLight() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override { return sizeof(FLightParticlePayload); }

	// 빛 색 (태어날 때 정함)
	UPROPERTY(EditAnywhere)
	FRawDistributionVector ColorScale = FRawDistributionVector::MakeConstant(FVector(1.0f, 1.0f, 1.0f));

	// 영향 반경 (태어날 때 정함)
	UPROPERTY(EditAnywhere)
	FRawDistributionFloat Radius = FRawDistributionFloat::MakeConstant(5.0f);

	// 기본 밝기. 점광원 컴포넌트의 Intensity와 같은 단위
	UPROPERTY(EditAnywhere)
	float Intensity = 5.0f;

	// 수명 비율(0 → 1)에 따른 밝기 배율. Curve로 깜빡임을 만들 수 있다
	UPROPERTY(EditAnywhere)
	FRawDistributionFloat BrightnessOverLife = FRawDistributionFloat::MakeConstant(1.0f);

	UPROPERTY(EditAnywhere)
	float FalloffExponent = 2.0f;

	// 빛을 낼 파티클 비율 (0 ~ 1). 파티클이 많을 때 일부만 빛내 비용을 줄인다
	UPROPERTY(EditAnywhere)
	float SpawnFraction = 1.0f;

	// 파티클 색·알파를 빛에 곱한다 (ColorOverLife로 사라지면 빛도 같이 꺼진다)
	UPROPERTY(EditAnywhere)
	bool bUseParticleColor = true;
};
