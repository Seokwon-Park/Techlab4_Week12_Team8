#include "EnginePCH.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Render/Material.h"
#include "Asset/AssetManager.h"

namespace
{
	template <typename T>
	void SerializeSubobject(FStructuredArchive::FRecord Record, T*& Object)
	{
		FArchive& Ar = Record.GetUnderlyingArchive();

		FString ClassName = (Ar.IsSaving() && Object) ? Object->GetClass()->Name : FString{};
		Record << SA_VALUE("Class", ClassName);

		if (Ar.IsLoading())
		{
			Object = nullptr;
			if (ClassName.empty()) return;

			UClass* Class = FindClass(ClassName);
			if (!Class || !Class->IsChildOf(T::StaticClass()))
			{
				HTR_LOG(Warning, "Particle: unknown module class '{}', skipped", ClassName);
				return;
			}

			Object = NewObject<T>(nullptr, Class);
		}

		if (Object)
		{
			Object->Serialize(Record.EnterRecord("Object"));
		}
	}
}

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

// ───────── 직렬화: 위에서 아래로 직렬화─────────
void UParticleSystem::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);
	FArchive& Ar = Record.GetUnderlyingArchive();

	int32 Version = Ar.IsSaving() ? 1 : 0;
	Record << SA_VALUE("Version", Version);
	if (Version != 1)
	{
		Ar.SetError();
		return;
	}

	int32 Count = Ar.IsSaving() ? static_cast<int32>(Emitters.Num()) : 0;
	auto Array = Record.EnterArray("Emitters", Count);

	if (Ar.IsLoading())
	{
		DeleteEmitters();          // 되돌리기(Revert)로 같은 객체에 다시 읽는 경우가 있다
	}

	for (int32 i = 0; i < Count; ++i)
	{
		if (Ar.IsLoading()) Emitters.Add(NewObject<UParticleEmitter>());
		Emitters[i]->Serialize(Array.EnterElement().EnterRecord());
	}

	if (Ar.IsLoading())
	{
		PostEditStructure();
	}
}

void UParticleEmitter::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);   // EmitterName
	FArchive& Ar = Record.GetUnderlyingArchive();

	int32 Count = Ar.IsSaving() ? static_cast<int32>(LODLevels.Num()) : 0;
	auto Array = Record.EnterArray("LODLevels", Count);

	for (int32 i = 0; i < Count; ++i)
	{
		if (Ar.IsLoading()) LODLevels.Add(NewObject<UParticleLODLevel>());
		LODLevels[i]->Serialize(Array.EnterElement().EnterRecord());
	}
}

void UParticleLODLevel::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);
	FArchive& Ar = Record.GetUnderlyingArchive();

	Record << SA_VALUE("Level", Level);

	bool bEnabledValue = bEnabled;                 // 비트필드는 참조가 안 된다
	Record << SA_VALUE("Enabled", bEnabledValue);
	if (Ar.IsLoading()) bEnabled = bEnabledValue;

	SerializeSubobject(Record.EnterRecord("Required"), RequiredModule);
	SerializeSubobject(Record.EnterRecord("TypeData"), TypeDataModule);

	int32 Count = Ar.IsSaving() ? static_cast<int32>(Modules.Num()) : 0;
	auto Array = Record.EnterArray("Modules", Count);

	TArray<UParticleModule*> Loaded;
	for (int32 i = 0; i < Count; ++i)
	{
		UParticleModule* Module = Ar.IsSaving() ? Modules[i] : nullptr;
		SerializeSubobject(Array.EnterElement().EnterRecord(), Module);
		if (Ar.IsLoading() && Module) Loaded.Add(Module);
	}

	if (Ar.IsLoading())
	{
		Modules = std::move(Loaded);
		SpawnModule = nullptr;     // 1번/2번: Modules에서 Spawn 모듈을 찾아 넣는다 (UpdateModuleLists에서 같이 해도 됨)

		if (!RequiredModule)       // 파일이 깨졌어도 에디터가 열리게 Required는 항상 있게 한다
		{
			RequiredModule = NewObject<UParticleModuleRequired>();
		}
	}
}