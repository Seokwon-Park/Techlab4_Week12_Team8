#pragma once

#include "GameFramework/Actor.h"
#include "Components/ParticleSystemComponent.h"
#include "ParticleEventManager.generated.h"

// World마다 하나씩 있는 파티클 이벤트 처리 액터
// PSC가 Tick 끝에 모은 이벤트를 넘기면, 이벤트마다 델리게이트를 Broadcast해 게임 코드에 알린다.
// 게임마다 처리를 바꾸고 싶으면 해당 클래스를 상속해 Handle 함수를 덮어쓴다.
UCLASS()
class AParticleEventManager : public AActor
{
	GENERATED_BODY()

public:
	AParticleEventManager() = default;
	virtual ~AParticleEventManager() override = default;

	// 지워질 때 월드가 해당 매니저를 가리키지 않도록 처리.
	virtual void EndPlay() override;

	// PSC가 이번 Tick의 충돌 이벤트를 넘김
	virtual void HandleParticleCollisionEvents(
		UParticleSystemComponent* Component, 
		const TArray<FParticleEventCollideData>& CollisionEvents
	);
};