#pragma once

// ============================================================================
// [Week12 0단계 공통 계약] UE의 Classes/Particles/ParticleLODLevel.h 에 대응한다.
// ============================================================================

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Particles/ParticleModule.h"
#include "ParticleLODLevel.generated.h"

class UParticleModuleRequired;
class UParticleModuleTypeDataBase;
class UParticleModuleSpawn;

UCLASS()
class UParticleLODLevel : public UObject
{
	GENERATED_BODY()

public:
	UParticleLODLevel() = default;

	/** The index value of the LOD level */
	int32 Level = 0;

	/** True if the LOD level is enabled, meaning it should be updated and rendered. */
	uint32 bEnabled:1 = true;

	/** The required module for this LOD level */
	UParticleModuleRequired* RequiredModule = nullptr;

	/** An array of particle modules that contain the adjusted data for the LOD level (Required, TypeData 제외) */
	TArray<UParticleModule*> Modules;

	// Module<SINGULAR> used for emitter type "extension".
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;

	/** The SpawnRate/Burst module - required by all emitters. */
	UParticleModuleSpawn* SpawnModule = nullptr;

	// 아래 두 목록은 UpdateModuleLists()가 Modules에서 만든다.
	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;

	// Modules가 바뀔 때마다 호출한다. Modules에서 bSpawnModule / bUpdateModule인 것을 순서대로 모은다.
	void UpdateModuleLists()
	{
		// 재호출 시 이전 목록이 남지 않도록 비우고 Modules 순서대로 다시 만든다.
		SpawnModules.Reset();
		UpdateModules.Reset();

		for (UParticleModule* Module : Modules)
		{
			if (!Module)
			{
				continue;
			}

			// 두 플래그는 독립적이므로 하나의 모듈이 두 목록에 모두 들어갈 수 있다.
			if (Module->bSpawnModule)
			{
				SpawnModules.Add(Module);
			}

			if (Module->bUpdateModule)
			{
				UpdateModules.Add(Module);
			}
		}
	}
};
