#pragma once

#include "Components/PrimitiveComponent.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleSystem.h"
#include "ParticleSystemComponent.generated.h"

struct FParticleEmitterInstance;
struct FDynamicEmitterDataBase;

// 충돌 이벤트 하나
struct FParticleEventCollideData
{
	FString EventName;			// 받는 쪽에서 해당 이름으로 고른다.
	float EmitterTime = 0.0f;	// 이벤트가 발생한 순간의 EmitterTime
	FVector Location;			// 충돌 지점
	FVector Velocity;			// 충돌 당시 파티클 속도
	float ParticleTime = 0.0f;	// 충돌 당시 파티클 RelativeTime
	FVector Direction;			// 진행 방향
	FVector Normal;				// 충돌 면 법선
	float Time = 0.0f;			// 이번 검사 구간의 몇 % 지점에서 충돌했는지 (0~1)
};

// 인자: 이벤트 이름, EmitterTime, 파티클 나이(RelativeTime), 위치, 속도, 진행 방향, 법선
DECLARE_DELEGATE(FParticleCollisionSignature, const FString&, float, float, const FVector&, const FVector&, const FVector&, const FVector&);

//Source/Runtime/Engine/Classes/Particles/ParticleSystemComponent.h 참고
//원래는 Cascade와 Niagara 를 통합관리하기 위한 UFXSystemComponent가 부모로 존재하지만 생략한다.
UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
public:
	UParticleSystemComponent();
	virtual ~UParticleSystemComponent() override;

	// ── 데이터 ──
	UPROPERTY(EditAnywhere)
	UParticleSystem* Template = nullptr;     // 에셋
	TArray<FParticleEmitterInstance*> EmitterInstances;              // 코어 소유
	TArray<FDynamicEmitterDataBase*>  EmitterRenderData;             // 렌더러가 읽는 스냅샷
	bool bWasCompleted = false;

	// ── 템플릿 / 생명주기 ──
	void SetTemplate(UParticleSystem* NewTemplate);   // 바꾸면 ResetParticles → InitializeSystem
	void InitializeSystem();                          // InitParticles + 초기 상태
	void InitParticles();                             // 이미터마다 TypeData->CreateInstance() 또는 Sprite 인스턴스 생성 → InitParameters → Init
	void ResetParticles();                            // 인스턴스와 RenderData 모두 delete
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;

	// 각 이미터의 MinDistance에 따라 전환한다. Tick에서는 월드의 활성 카메라를 자동으로 사용한다.
	// 별도 카메라를 쓰는 호출자는 이 함수로 위치를 직접 전달할 수 있다.
	void UpdateLODLevel(const FVector& ViewLocation);

	// ── 엔진 훅 (override) ──
	virtual void BeginPlay() override;                // bAutoActivate면 Activate
	virtual void TickComponent(float DeltaTime) override;  // 인스턴스 Tick → UpdateDynamicData → 바운드 갱신
	virtual void OnUnregister() override;             // ResetParticles
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBox CalcLocalBounds() const override;    // 파티클 위치로 계산 (컬링용)
	virtual void PostEditChangeProperty(const FProperty& Property) override; // Template 변경 시 리셋

	// ── 렌더 경계 ──
	void UpdateDynamicData();                         // 이전 RenderData delete -> 인스턴스마다 GetDynamicData(bSelected)
	void ClearDynamicData();

	// EventGenerator가 충돌 하나를 기록할 때 호출
	void ReportEventCollision(
		const FString& InEventName,
		float InEmitterTime,
		const FVector& InLocation,
		const FVector& InDirection,
		const FVector& InVelocity,
		float InParticleTime,
		const FVector& InNormal,
		float InTime
	);

	// ── Stat ──
	int32 GetNumActiveParticles() const;              // ActiveParticles 합계

	FParticleCollisionSignature OnParticleCollide;

	// 이번 Tick 동안 EventGenerator가 기록한 충돌. Tick 시작 때 비운다.
	TArray<FParticleEventCollideData> CollisionEvents;

	////////////////////파티클 테스팅 용 코드(지워도 됨)
	UPROPERTY(EditAnywhere)
	bool bUseRenderTestData = false;
	UMaterial* RenderTestMaterial = nullptr;   // 7단계에서 만든다
	float RenderTestTime = 0.0f;

	////////////////////파티클 테스팅 용 코드(지워도 됨)

private:
	void UpdateLODLevelFromCamera();
	void UpdateCompletion();
};
