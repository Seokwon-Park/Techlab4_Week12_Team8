#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleSpawn.generated.h"

USTRUCT()
struct FParticleBurst
{
	GENERATED_BODY()

	// Burst할 파티클 개수. 0 이하면 비어 있는 슬롯
	UPROPERTY(EditAnywhere)
	int32 Count = 0;

	// 범위 [CountLow, Coun]. -1 이면 Count개 고정
	UPROPERTY(EditAnywhere)
	int32 CountLow = -1;

	// 루프 시작부터 흐른 초
	UPROPERTY(EditAnywhere)
	float Time = 0.0f;
};

struct FParticleBurstFired;

UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
	GENERATED_BODY()

public:
	// TODO: 리플렉션 배열 지원 (현재는 슬롯 개수 0~3개로 고정)
	static constexpr int32 MaxBursts = 4;

	UParticleModuleSpawn() = default;
	virtual ~UParticleModuleSpawn() override = default;

	int32 GetBurstCount(float EmitterTime, FParticleBurstFired& InOutFired) const;

	UPROPERTY(EditAnywhere)
	float Rate = 20.0f;

	UPROPERTY(EditAnywhere)
	FParticleBurst Burst0;

	UPROPERTY(EditAnywhere)
	FParticleBurst Burst1;

	UPROPERTY(EditAnywhere)
	FParticleBurst Burst2;

	UPROPERTY(EditAnywhere)
	FParticleBurst Burst3;
};

// Burst 슬롯별 발사 여부. Emitter 인스턴스마다 하나씩.
struct FParticleBurstFired
{
	bool Fired[UParticleModuleSpawn::MaxBursts] = {};

	void Reset()
	{
		for (bool& bFired : Fired)
		{
			bFired = false;
		}
	}
};
