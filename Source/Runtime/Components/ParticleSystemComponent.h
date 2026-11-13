#pragma once

#include "Components/PrimitiveComponent.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleSystem.h"
#include "ParticleSystemComponent.generated.h"

struct FParticleEmitterInstance;
struct FDynamicEmitterDataBase;

//Source/Runtime/Engine/Classes/Particles/ParticleSystemComponent.h 참고
//원래는 Cascade와 Niagara 를 통합관리하기 위한 UFXSystemComponent가 부모로 존재하지만 생략한다.
UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
public:
	// ── 데이터 ──
	UPROPERTY(EditAnywhere)
	UParticleSystem* Template = nullptr;     // 에셋
	UPROPERTY(EditAnywhere)
	bool bAutoActivate = true;
	TArray<FParticleEmitterInstance*> EmitterInstances;              // 코어 소유
	TArray<FDynamicEmitterDataBase*>  EmitterRenderData;             // 렌더러가 읽는 스냅샷
	bool bWasCompleted = false;

	// ── 템플릿 / 생명주기 ──
	void SetTemplate(UParticleSystem* NewTemplate);   // 바꾸면 ResetParticles → InitializeSystem
	void InitializeSystem();                          // InitParticles + 초기 상태
	void InitParticles();                             // 이미터마다 TypeData->CreateInstance() 또는 Sprite 인스턴스 생성 → InitParameters → Init
	void ResetParticles();                            // 인스턴스와 RenderData 모두 delete
	void Activate(bool bReset = false);
	void Deactivate();

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

	// ── Stat ──
	int32 GetNumActiveParticles() const;              // ActiveParticles 합계

	// (선택) Collision 이벤트
	// TArray<FParticleEventCollideData> CollisionEvents;
};