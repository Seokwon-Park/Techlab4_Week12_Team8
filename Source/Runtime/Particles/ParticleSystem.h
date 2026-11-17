#pragma once

// ============================================================================
// [Week12 0단계 공통 계약] UE의 Classes/Particles/ParticleSystem.h 에 대응한다.
//
//   UParticleSystem ─ Emitters[] ─ UParticleEmitter ─ LODLevels[] ─ UParticleLODLevel ─ Modules[]
//
// 에디터는 이 계층을 편집하고, 코어는 이 계층을 읽어 FParticleEmitterInstance를 만든다.
// ============================================================================

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "ParticleSystem.generated.h"

class UParticleEmitter;

UCLASS()
class UParticleSystem : public UObject
{
	GENERATED_BODY()

public:
	UParticleSystem() = default;
	virtual ~UParticleSystem() override;

	/** Emitters	- internal - the array of emitters in the system */
	TArray<UParticleEmitter*> Emitters;

	using Super::Serialize;
	void Serialize(FStructuredArchive::FRecord Record) override;

	void PostEditStructure();

	void DeleteEmitters();
	const FString& GetAssetPath() const { return AssetPath; }
	void SetAssetPath(const FString& InPath) { AssetPath = InPath; }
private:
	FString AssetPath;
};
