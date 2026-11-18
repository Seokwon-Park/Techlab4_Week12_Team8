#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleEventGenerator.generated.h"

struct FHitResult;
struct FParticleCollisionPayload;

// 이벤트 종류
UENUM()
enum class EParticleEventType : uint8
{
	EPET_Any,
	EPET_Spawn,
	EPET_Death,
	EPET_Collision,
	EPET_Burst,
	EPET_MAX UMETA(Hidden)
};

USTRUCT()
struct FParticleEvent_GenerateInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	EParticleEventType Type = EParticleEventType::EPET_Collision;

	// 받는 쪽에서 해당 이름으로 이벤트를 선택
	// 언리얼 엔진은 FName이지만 현재 Reflection System에서 FString만 지원함
	UPROPERTY(EditAnywhere)
	FString CustomName;

	// 파티클의 첫 충돌만 알림
	UPROPERTY(EditAnywhere)
	bool FirstTimeOnly = false;

	// 파티클의 마지막 충돌만 알림
	UPROPERTY(EditAnywhere)
	bool LastTimeOnly = false;
};

UCLASS()
class UParticleModuleEventGenerator : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleEventGenerator() = default;
	virtual ~UParticleModuleEventGenerator() override = default;

	// Collision Module이 충돌을 처리하는 중에 호출. 조건에 맞으면 PSC에 충돌 이벤트를 기록.
	bool HandleParticleCollision(
		FParticleEmitterInstance* Owner,
		const FParticleCollisionPayload& CollidePayload,
		const FHitResult& HitResult,
		const FBaseParticle& CollideParticle,
		const FVector& CollideDirection
	) const;

	// Reflection System에서 배열을 지원하지 않아서 우선 한칸 고정
	UPROPERTY(EditAnywhere)
	FParticleEvent_GenerateInfo Event0;
};