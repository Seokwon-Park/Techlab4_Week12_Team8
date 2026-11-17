#include "EnginePCH.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Render/Material.h"
#include "UObject/Casts.h"
#include "Asset/AssetManager.h"

namespace
{
	void SerializeDistributionCurves(FStructuredArchive::FRecord Record, UObject* Object);   // 아래에 정의

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
			SerializeDistributionCurves(Record, Object);
		}
	}

	void SerializeCurve(FStructuredArchive::FRecord Record, const char* Name, FParticleCurve& Curve)
	{
		FArchive& Ar = Record.GetUnderlyingArchive();
		if (Ar.IsSaving() && Curve.Keys.IsEmpty()) return;

		TOptional<FStructuredArchive::FSlot> Field = Record.TryEnterField(Name, true);
		if (!Field.IsSet()) return;                     // 불러오기: 이 커브가 없는 파일 (키 없음 또는 옛 파일)

		int32 Count = Ar.IsSaving() ? static_cast<int32>(Curve.Keys.Num()) : 0;
		auto Array = Field.GetValue().EnterArray(Count);
		if (Ar.IsLoading()) Curve.Keys.SetNum(Count);

		for (int32 i = 0; i < Count; ++i)
		{
			auto KeyRecord = Array.EnterElement().EnterRecord();
			FParticleCurveKey& Key = Curve.Keys[i];
			int32 Mode = static_cast<int32>(Key.InterpMode);

			KeyRecord << SA_VALUE("T", Key.Time);
			KeyRecord << SA_VALUE("V", Key.Value);
			KeyRecord << SA_VALUE("Mode", Mode);
			KeyRecord << SA_VALUE("Auto", Key.bAutoTangent);
			KeyRecord << SA_VALUE("Arrive", Key.ArriveTangent);
			KeyRecord << SA_VALUE("Leave", Key.LeaveTangent);

			if (Ar.IsLoading()) Key.InterpMode = static_cast<EParticleCurveInterp>(std::clamp(Mode, 0, 2));
		}
	}

	// 객체의 UPROPERTY 중 Distribution 구조체를 찾아 그 커브를 "Curves"에 쓴다
	void SerializeDistributionCurves(FStructuredArchive::FRecord Record, UObject* Object)
	{
		TOptional<FStructuredArchive::FSlot> CurvesField = Record.TryEnterField("Curves", true);
		if (!CurvesField.IsSet()) return;
		FStructuredArchive::FRecord Curves = CurvesField.GetValue().EnterRecord();

		for (const UStruct* Struct = Object->GetClass(); Struct; Struct = Struct->GetSuperStruct())
		{
			for (const FProperty* Property : Struct->GetProperties())
			{
				const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
				if (!StructProperty) continue;
				void* Value = Property->ContainerPtrToValuePtr(Object);

				if (StructProperty->Struct == FRawDistributionFloat::StaticStruct())
				{
					SerializeCurve(Curves, Property->Name.c_str(), static_cast<FRawDistributionFloat*>(Value)->Curve);
				}
				else if (StructProperty->Struct == FRawDistributionVector::StaticStruct())
				{
					auto* Distribution = static_cast<FRawDistributionVector*>(Value);
					TOptional<FStructuredArchive::FSlot> Axes = Curves.TryEnterField(Property->Name.c_str(), true);
					if (!Axes.IsSet()) continue;
					FStructuredArchive::FRecord AxesRecord = Axes.GetValue().EnterRecord();
					SerializeCurve(AxesRecord, "X", Distribution->Curves[0]);
					SerializeCurve(AxesRecord, "Y", Distribution->Curves[1]);
					SerializeCurve(AxesRecord, "Z", Distribution->Curves[2]);
				}
			}
		}
	}

}

// ───────── 소유권 ─────────
// 엔진 종료 때 FEngineLoop::DestroyAllObjectsExceptEngine()이 GUObjectArray의 모든 UObject를
// 끝에서부터 한 번씩 delete한다. 소멸자에서 자식까지 지우면 이미 지워진 자식을 다시 지우게 되므로
// 소멸자는 비워 두고, 실행 중에 계층을 지울 때만 아래 Destroy 함수로 명시적으로 지운다.
UParticleSystem::~UParticleSystem() = default;
UParticleEmitter::~UParticleEmitter() = default;
UParticleLODLevel::~UParticleLODLevel() = default;
UParticleModuleRequired::~UParticleModuleRequired() = default;

void UParticleModuleRequired::ReleaseRenderMaterial()
{
	delete CachedMaterial;
	CachedMaterial = nullptr;
}

void UParticleSystem::DestroyModule(UParticleModule* Module)
{
	if (!Module) return;
	if (UParticleModuleRequired* Required = Cast<UParticleModuleRequired>(Module))
	{
		Required->ReleaseRenderMaterial();
	}
	delete Module;
}

void UParticleSystem::DestroyEmitter(UParticleEmitter* Emitter)
{
	if (!Emitter) return;
	for (UParticleLODLevel* LOD : Emitter->LODLevels)
	{
		if (!LOD) continue;
		DestroyModule(LOD->RequiredModule);
		DestroyModule(LOD->TypeDataModule);
		for (UParticleModule* Module : LOD->Modules) DestroyModule(Module);   // SpawnModule은 이 안에 있다
		delete LOD;
	}
	delete Emitter;
}

void UParticleSystem::DeleteEmitters()
{
	for (UParticleEmitter* Emitter : Emitters) DestroyEmitter(Emitter);
	Emitters.Reset();
}

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
			if (!LOD) continue;
			LOD->UpdateModuleLists();

			// 코어의 Spawn()은 SpawnModule이 없으면 파티클을 하나도 만들지 않는다.
			// UE처럼 Modules 안의 첫 Spawn 모듈을 가리키게 한다 (소유는 Modules)
			LOD->SpawnModule = nullptr;
			for (UParticleModule* Module : LOD->Modules)
			{
				if (UParticleModuleSpawn* Spawn = Cast<UParticleModuleSpawn>(Module))
				{
					LOD->SpawnModule = Spawn;
					break;
				}
			}
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