#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleCollision.generated.h"

UENUM()
enum class EParticleCollisionComplete : uint8
{
	EPCC_Kill,				// 제거
	EPCC_Freeze,			// 모든 업데이트 중지
	EPCC_HaltCollisions,	// 충돌 검사만 중지
	EPCC_FreezeTranslation,	// 이동만 중지
	EPCC_FreezeRotation,	// 회전만 중지
	EPCC_FreezeMovement,	// 이동과 회전 중지
	EPCC_MAX UMETA(Hidden)
};

struct FParticleCollisionPayload
{
	FVector UsedDampingFactor;
	int32 UsedCollisions;
	float Delay;
};

UCLASS()
class UParticleModuleCollision : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleCollision();
	virtual ~UParticleModuleCollision() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	virtual uint32	RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;

	UPROPERTY(EditAnywhere)
	FRawDistributionVector DampingFactor;

	UPROPERTY(EditAnywhere)
	FRawDistributionFloat MaxCollisions;

	UPROPERTY(EditAnywhere)
	EParticleCollisionComplete CollisionCompletionOption = EParticleCollisionComplete::EPCC_Kill;

	// 진행 방향으로 파티클 크기의 1 / DirScalar만큼 더 앞을 검사
	UPROPERTY(EditAnywhere)
	float DirScalar = 3.5f;

	// 충돌 검사 시작 전 기다릴 수명 비율
	UPROPERTY(EditAnywhere)
	FRawDistributionFloat DelayAmount;
};