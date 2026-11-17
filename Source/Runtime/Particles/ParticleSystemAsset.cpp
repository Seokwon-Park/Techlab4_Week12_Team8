#include "EnginePCH.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Render/Material.h"
#include "Asset/AssetManager.h"

// ───────── 소멸자: 위에서 아래로 delete ─────────
UParticleSystem::~UParticleSystem() { DeleteEmitters(); }

void UParticleSystem::DeleteEmitters()
{
	for (UParticleEmitter* Emitter : Emitters) delete Emitter;
	Emitters.Reset();
}

UParticleEmitter::~UParticleEmitter()
{
	for (UParticleLODLevel* LOD : LODLevels) delete LOD;
}

UParticleLODLevel::~UParticleLODLevel()
{
	delete RequiredModule;
	delete TypeDataModule;
	for (UParticleModule* Module : Modules) delete Module;   // SpawnModule은 이 안에 있으므로 따로 지우지 않는다
}

UParticleModuleRequired::~UParticleModuleRequired() { delete CachedMaterial; }

// ───────── 렌더용 머티리얼 ─────────
UMaterialInterface* UParticleModuleRequired::GetRenderMaterial()
{
	if (Material) return Material;

	if (!CachedMaterial)
	{
		UMaterial* Base = UAssetManager::GetAssetByPath<UMaterial>("SubUVMaterial");
		CachedMaterial = UMaterial::CreateInstance(Base);
		if (!CachedMaterial) return nullptr;
		CachedMaterial->SamplerState = ESamplerState::LinearClamp;
	}

	// 값이 바뀌어도 바로 반영되도록 매번 덮어쓴다 (대입 두 번이라 비용 없음)
	CachedMaterial->PSOType = bAdditive ? EPSOType::ParticleSprite_Additive : EPSOType::ParticleSprite_AlphaBlend;
	if (Texture)
	{
		if (CachedMaterial->Textures.IsEmpty()) CachedMaterial->Textures.Add(Texture);
		else CachedMaterial->Textures[0] = Texture;
	}
	return CachedMaterial;
}

// ───────── 구성이 바뀐 뒤 캐시 다시 만들기 ─────────
void UParticleSystem::PostEditStructure()
{
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (!Emitter) continue;
		for (UParticleLODLevel* LOD : Emitter->LODLevels)
		{
			if (LOD) LOD->UpdateModuleLists();        
		}
		Emitter->CacheEmitterModuleInfo();            
	}
}