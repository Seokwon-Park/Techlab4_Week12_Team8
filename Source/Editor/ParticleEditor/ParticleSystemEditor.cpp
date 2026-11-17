#include "EnginePCH.h"
#include "ParticleSystemEditor.h"
#include "ParticleSystemEditorTab.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Particles/ParticleSystemFile.h"
#include "Asset/AssetUtils.h"
#include "Engine/World.h"
#include "Render/LineBatcher.h"
#include "Math/BoxSphereBounds.h"
#include "UObject/Casts.h"
#include "UObject/UObjectIterator.h"
#include "Editor/Details/PropertyEditor.h"

namespace
{
	bool IsFloatDistribution(const FProperty* Property)
	{
		const FStructProperty* S = CastField<FStructProperty>(Property);
		return S && S->Struct == FRawDistributionFloat::StaticStruct();
	}
	bool IsVectorDistribution(const FProperty* Property)
	{
		const FStructProperty* S = CastField<FStructProperty>(Property);
		return S && S->Struct == FRawDistributionVector::StaticStruct();
	}

	// 모듈의 Distribution 속성을 클래스 → 부모 순서로 하나씩 넘긴다
	template <typename TFunc>
	void ForEachDistribution(const UParticleModule* Module, TFunc&& Func)
	{
		for (const UStruct* Struct = Module->GetClass(); Struct; Struct = Struct->GetSuperStruct())
			for (const FProperty* Property : Struct->GetProperties())
				if (IsFloatDistribution(Property) || IsVectorDistribution(Property))
					Func(static_cast<const FStructProperty*>(Property));
	}

	// 디테일 패널: Distribution은 고른 Type에 쓰이는 칸만 보인다 (UE의 Distribution 드롭다운처럼)
	//   Constant      → Constant
	//   Uniform       → Min, Max (+ Vector는 LockedAxes, MirrorX/Y/Z)
	//   ConstantCurve → CurveStart, CurveEnd
	//   Curve         → 없음 (키는 Curve Editor에서)
	FPropertyEditor::FStructMemberDisplay DistributionMemberDisplay(ERawDistributionType Type, const FProperty& Member)
	{
		const FString& Name = Member.Name;
		if (Name == "Type") return { true, "Distribution" };

		bool bVisible = false;
		switch (Type)
		{
		case ERawDistributionType::Constant:
			bVisible = Name == "Constant";
			break;
		case ERawDistributionType::Uniform:
			bVisible = Name == "Min" || Name == "Max" || Name == "LockedAxes"
				|| Name == "MirrorX" || Name == "MirrorY" || Name == "MirrorZ";
			break;
		case ERawDistributionType::ConstantCurve:
			bVisible = Name == "CurveStart" || Name == "CurveEnd";
			break;
		default:
			break;
		}
		return { bVisible, nullptr };
	}

	void RegisterDistributionDetails()
	{
		static bool bRegistered = false;
		if (bRegistered) return;
		bRegistered = true;

		FPropertyEditor::RegisterStructMemberRule(FRawDistributionFloat::StaticStruct(),
			[](const void* Value, const FProperty& Member)
			{ return DistributionMemberDisplay(static_cast<const FRawDistributionFloat*>(Value)->GetType(), Member); });
		FPropertyEditor::RegisterStructMemberRule(FRawDistributionVector::StaticStruct(),
			[](const void* Value, const FProperty& Member)
			{ return DistributionMemberDisplay(static_cast<const FRawDistributionVector*>(Value)->GetType(), Member); });
	}
}

FParticleSystemEditor::FParticleSystemEditor(UEditorEngine& InEditorEngine, FEditorManager& InEditorManager,
	const FString& InAssetPath, UParticleSystem* InSystem)
	: FAssetEditor(InEditorEngine, InEditorManager, InAssetPath)
	, System(InSystem)
{
}

bool FParticleSystemEditor::InitializeResources()
{
	if (!System || !FAssetEditor::InitializeResources())   // PreviewScene + ViewportClient + Viewport
	{
		return false;
	}
	RegisterDistributionDetails();

	PreviewComponent = Cast<UParticleSystemComponent>(PreviewScene.AddComponent(UParticleSystemComponent::StaticClass()));
	if (!PreviewComponent)
	{
		return false;
	}

	// SetTemplate은 1번 구현에 따라 동작이 달라지므로 직접 넣고 재시작한다
	PreviewComponent->Template = System;
	RestartPreview();
	FocusPreview();
	return true;
}

TUniquePtr<IEditorTab> FParticleSystemEditor::CreateTab()
{
	return MakeUnique<FParticleSystemEditorTab>(*this);
}

void FParticleSystemEditor::ShutdownResources()
{
	SelectedObject = nullptr;
	PendingEdits.Reset();
	PreviewComponent = nullptr;          // 월드가 지워질 때 PSC::OnUnregister가 정리한다
	FAssetEditor::ShutdownResources();
	System = nullptr;                    // 캐시 소유이므로 delete하지 않는다
}

void FParticleSystemEditor::Tick(float DeltaTime)
{
	if (!ViewportClient || !ViewportClient->GetWorld())
	{
		return;
	}

	// 1) 패널이 지난 프레임에 쌓은 편집을 적용 (월드 Tick 전에)
	ApplyPendingEdits();

	// 2) 카메라 (FStaticMeshEditor::Tick과 같음)
	if (bViewportVisible)
	{
		FViewportCameraState Camera{
			.Location = ViewportClient->GetViewLocation(),
			.Rotation = ViewportClient->GetViewRotation(),
			.OrthoWidth = ViewportClient->GetOrthoWidth(),
			.ViewWidth = static_cast<float>(ViewportWidth),
			.bPerspective = true
		};
		ApplyViewportCameraMovement(Camera, CameraInput, DeltaTime, 5.0f, 0.2f);
		ViewportClient->SetViewLocation(Camera.Location);
		ViewportClient->SetViewRotation(Camera.Rotation);
	}
	CameraInput = {};

	// 3) 미리보기 월드 Tick → PSC Tick (5단계)
	FAssetEditor::Tick(DeltaTime);
}

void FParticleSystemEditor::OnRenderPreview()
{
	// FStaticMeshEditor::OnRenderPreview와 같다
	const bool bRender = bViewportVisible;
	bViewportVisible = false;
	if (!bRender || !ViewportClient || !ViewportClient->GetWorld() || !Viewport)
	{
		return;
	}

	Viewport->UpdateViewportRHI(false, ViewportWidth, ViewportHeight);
	if (!Viewport->GetViewTargets().IsValidFor(ViewportWidth, ViewportHeight))
	{
		return;
	}

	if (bInitialFocusPending)
	{
		FocusPreview();
		bInitialFocusPending = false;
	}
	Viewport->Draw();
}

void FParticleSystemEditor::SetViewportState(uint32 Width, uint32 Height, const FViewportCameraInput& Input)
{
	ViewportWidth = Width;
	ViewportHeight = Height;
	bViewportVisible = Width > 0 && Height > 0;
	CameraInput = Input;
}

void FParticleSystemEditor::FocusPreview()
{
	if (!ViewportClient || !PreviewComponent) return;

	// 바운드는 살아 있는 파티클로 계산된다. 막 재시작해서 파티클이 없으면 크기 0이라
	// 카메라가 원점에 0.2만큼 붙어 버리므로, 작으면 기본 상자(±3)로 맞춘다
	FBox Box = PreviewComponent->CalcLocalBounds();
	if ((Box.Max - Box.Min).Size() < 1.0f)
	{
		Box = FBox{ FVector(-3.0f, -3.0f, -3.0f), FVector(3.0f, 3.0f, 3.0f) };
	}
	const FVector Extent = (Box.Max - Box.Min) * 0.5f;
	ViewportClient->FocusOnBounds(FBoxSphereBounds{
		.Origin = (Box.Min + Box.Max) * 0.5f,
		.BoxExtent = Extent,
		.SphereRadius = Extent.Size() });
}

void FParticleSystemEditor::RestartPreview()
{
	if (!PreviewComponent) return;
	PreviewComponent->ResetParticles();
	PreviewComponent->ClearDynamicData();
	PreviewComponent->InitializeSystem();
	PreviewComponent->Activate(true);
}

bool FParticleSystemEditor::IsShowingTestData() const
{
	return PreviewComponent && PreviewComponent->bUseRenderTestData;
}

void FParticleSystemEditor::SetShowTestData(bool bShow)
{
	if (PreviewComponent) PreviewComponent->bUseRenderTestData = bShow;
}

// FAssetEditorViewportClient::Draw가 미리보기 위에 선을 그릴 때 부른다 (SkeletalPreviewEditor와 같은 자리)
void FParticleSystemEditor::OnRenderLine(FLineBatcher& LineBatcher)
{
	if (!PreviewComponent) return;

	if (bShowOriginAxis)   // 미리보기 PSC는 원점에 있다
	{
		LineBatcher.AddLine(FVector(0, 0, 0), FVector(1, 0, 0), FVector4(1.0f, 0.2f, 0.2f, 1.0f));
		LineBatcher.AddLine(FVector(0, 0, 0), FVector(0, 1, 0), FVector4(0.2f, 1.0f, 0.2f, 1.0f));
		LineBatcher.AddLine(FVector(0, 0, 0), FVector(0, 0, 1), FVector4(0.3f, 0.5f, 1.0f, 1.0f));
	}
	if (bShowBounds)
	{
		LineBatcher.AddBox(PreviewComponent->CalcLocalBounds(), FVector4(1.0f, 0.8f, 0.2f, 1.0f));
	}
}

// 이미터 i의 인스턴스 = PSC->EmitterInstances[i] (1번 InitParticles가 이미터 순서대로 만든다)
int32 FParticleSystemEditor::GetActiveParticles(int32 EmitterIndex) const
{
	if (!PreviewComponent || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(PreviewComponent->EmitterInstances.Num())) return 0;
	const FParticleEmitterInstance* Instance = PreviewComponent->EmitterInstances[EmitterIndex];
	return Instance ? Instance->ActiveParticles : 0;
}

int32 FParticleSystemEditor::GetMaxActiveParticles(int32 EmitterIndex) const
{
	if (!PreviewComponent || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(PreviewComponent->EmitterInstances.Num())) return 0;
	const FParticleEmitterInstance* Instance = PreviewComponent->EmitterInstances[EmitterIndex];
	return Instance ? Instance->MaxActiveParticles : 0;
}

UParticleLODLevel* FParticleSystemEditor::GetLOD(int32 EmitterIndex) const
{
	if (!System || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(System->Emitters.Num())) return nullptr;
	UParticleEmitter* Emitter = System->Emitters[EmitterIndex];
	return (Emitter && !Emitter->LODLevels.IsEmpty()) ? Emitter->LODLevels[0] : nullptr;
}

void FParticleSystemEditor::Select(int32 EmitterIndex, UObject* Object)
{
	SelectedEmitter = EmitterIndex;
	SelectedObject = Object;
}

void FParticleSystemEditor::RequestAddEmitter()
{
	PendingEdits.Add([this]
		{
			UParticleEmitter* Emitter = ParticleSystemFile::CreateDefaultEmitter();
			System->Emitters.Add(Emitter);
			Select(static_cast<int32>(System->Emitters.Num()) - 1, Emitter);
		});
}

void FParticleSystemEditor::RequestRemoveEmitter(int32 EmitterIndex)
{
	PendingEdits.Add([this, EmitterIndex]
		{
			if (EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(System->Emitters.Num())) return;
			UParticleEmitter* Emitter = System->Emitters[EmitterIndex];
			System->Emitters.RemoveAt(EmitterIndex);
			Select(-1, nullptr);                 // 선택이 지울 객체를 가리킬 수 있다
			UParticleSystem::DestroyEmitter(Emitter);   // LOD·모듈까지. 이 시점엔 PSC가 모두 리셋돼 있다
		});
}

void FParticleSystemEditor::RequestAddModule(int32 EmitterIndex, UClass* ModuleClass)
{
	PendingEdits.Add([this, EmitterIndex, ModuleClass]
		{
			UParticleLODLevel* LOD = GetLOD(EmitterIndex);
			if (!LOD || !ModuleClass) return;
			UParticleModule* Module = NewObject<UParticleModule>(nullptr, ModuleClass);
			LOD->Modules.Add(Module);
			Select(EmitterIndex, Module);
		});
}

void FParticleSystemEditor::RequestRemoveModule(int32 EmitterIndex, UParticleModule* Module)
{
	PendingEdits.Add([this, EmitterIndex, Module]
		{
			UParticleLODLevel* LOD = GetLOD(EmitterIndex);
			if (!LOD) return;
			for (int32 i = 0; i < static_cast<int32>(LOD->Modules.Num()); ++i)
			{
				if (LOD->Modules[i] != Module) continue;
				LOD->Modules.RemoveAt(i);
				if (reinterpret_cast<UParticleModule*>(LOD->SpawnModule) == Module) LOD->SpawnModule = nullptr;
				if (SelectedObject == Module) Select(EmitterIndex, nullptr);
				RemoveCurveTracks(Module);
				UParticleSystem::DestroyModule(Module);
				return;
			}
		});
}

void FParticleSystemEditor::RequestMoveModule(int32 EmitterIndex, int32 From, int32 To)
{
	PendingEdits.Add([this, EmitterIndex, From, To]
		{
			UParticleLODLevel* LOD = GetLOD(EmitterIndex);
			const int32 Num = LOD ? static_cast<int32>(LOD->Modules.Num()) : 0;
			if (From < 0 || To < 0 || From >= Num || To >= Num) return;
			std::swap(LOD->Modules[From], LOD->Modules[To]);   // 순서 = Spawn/Update 실행 순서
		});
}

void FParticleSystemEditor::RequestSetTypeData(int32 EmitterIndex, UClass* TypeDataClass)
{
	PendingEdits.Add([this, EmitterIndex, TypeDataClass]
		{
			UParticleLODLevel* LOD = GetLOD(EmitterIndex);
			if (!LOD) return;
			if (SelectedObject == LOD->TypeDataModule) Select(EmitterIndex, nullptr);
			UParticleSystem::DestroyModule(LOD->TypeDataModule);
			LOD->TypeDataModule = TypeDataClass ? NewObject<UParticleModuleTypeDataBase>(nullptr, TypeDataClass) : nullptr;
		});
}

void FParticleSystemEditor::RequestDeleteSelected()
{
	UParticleLODLevel* LOD = GetLOD(SelectedEmitter);
	if (!SelectedObject || !LOD) return;

	if (SelectedObject == System->Emitters[SelectedEmitter]) RequestRemoveEmitter(SelectedEmitter);
	else if (SelectedObject == LOD->TypeDataModule) RequestSetTypeData(SelectedEmitter, nullptr);
	else if (SelectedObject != LOD->RequiredModule)                     // Required는 지울 수 없다
	{
		if (UParticleModule* Module = Cast<UParticleModule>(SelectedObject))
			RequestRemoveModule(SelectedEmitter, Module);
	}
}

void FParticleSystemEditor::NotifyPropertyChanged()
{
	bDirty = true;
	bRestartPending = true;     // 드래그가 끝난 뒤 한 번 재시작 (ApplyPendingEdits)
}

void FParticleSystemEditor::ApplyPendingEdits()
{
	const bool bHasEdits = !PendingEdits.IsEmpty();
	// 슬라이더를 끄는 동안 매 프레임 재시작하면 미리보기가 계속 처음으로 돌아간다
	const bool bRestartNow = bRestartPending && !ImGui::IsAnyItemActive();
	if (!bHasEdits && !bRestartNow) return;

	ResetComponentsUsingSystem();            // ① 지울 모듈을 가리키는 인스턴스부터 없앤다

	TArray<std::function<void()>> Edits = std::move(PendingEdits);
	PendingEdits = {};
	for (auto& Edit : Edits) Edit();         // ②

	System->PostEditStructure();             // ③ 모듈 목록·페이로드 offset
	ReinitComponentsUsingSystem();           // ④

	if (bHasEdits) bDirty = true;
	bRestartPending = false;
}

// 같은 Template을 쓰는 PSC 전부: 미리보기 + 레벨 에디터 월드 + 다른 미리보기
void FParticleSystemEditor::ResetComponentsUsingSystem()
{
	for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
	{
		if (It->Template != System) continue;
		It->ResetParticles();
		It->ClearDynamicData();      // 렌더 데이터가 지울 머티리얼(Required::CachedMaterial)을 가리킬 수 있다
	}
}

void FParticleSystemEditor::ReinitComponentsUsingSystem()
{
	for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
	{
		if (It->Template != System || !It->IsRegistered()) continue;
		It->InitializeSystem();
		It->Activate(true);
	}
}

FString FParticleSystemEditor::GetFilePath() const
{
	const FString FilePath = FAssetUtils::ResolveAssetFilePath(GetAssetPath());
	return FilePath.empty() ? GetAssetPath() : FilePath;
}

bool FParticleSystemEditor::Save()
{
	if (!System) return false;
	FString Error;
	if (!ParticleSystemFile::Write(GetFilePath(), *System, Error))
	{
		HTR_LOG(Warning, "Failed to save particle system: {} ({})", GetAssetPath(), Error);
		return false;
	}
	bDirty = false;
	HTR_LOG(Info, "Saved particle system: {}", GetAssetPath());
	return true;
}

bool FParticleSystemEditor::ConfirmClose()
{
	if (!bDirty) return true;

	const int Result = MessageBoxW(GetActiveWindow(), L"Save changes to this particle system?",
		L"Particle System", MB_YESNOCANCEL | MB_ICONQUESTION);
	if (Result == IDCANCEL) return false;
	if (Result == IDYES) return Save();

	RevertFromDisk();   // 캐시 객체가 레벨과 공유되므로, 저장 안 한 편집을 남겨 두면 안 된다
	return true;
}

void FParticleSystemEditor::RevertFromDisk()
{
	CurveTracks.Reset();
	PendingEdits.Reset();
	Select(-1, nullptr);
	ResetComponentsUsingSystem();

	FString Error;
	if (!ParticleSystemFile::Read(GetFilePath(), *System, Error))   // Serialize가 기존 이미터를 지우고 다시 만든다
	{
		HTR_LOG(Warning, "Failed to revert particle system: {} ({})", GetAssetPath(), Error);
	}

	ReinitComponentsUsingSystem();
	bDirty = false;
}

void FParticleSystemEditor::HandleShortcuts()
{
	// 패널 창 안에서 부른다. 입력 중인 텍스트 칸이 있으면 무시
	if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::IsAnyItemActive()) return;

	const ImGuiIO& IO = ImGui::GetIO();
	if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) Save();
	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) RequestDeleteSelected();
	if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) RestartPreview();
}

FParticleCurve* FParticleSystemEditor::FCurveTrack::GetChannel(int32 Channel) const
{
	void* Value = Property->ContainerPtrToValuePtr(Module);
	if (NumChannels == 1) return &static_cast<FRawDistributionFloat*>(Value)->Curve;
	return &static_cast<FRawDistributionVector*>(Value)->Curves[Channel];
}

bool FParticleSystemEditor::FCurveTrack::IsCurveType() const
{
	const void* Value = Property->ContainerPtrToValuePtr(Module);
	const ERawDistributionType Type = NumChannels == 1
		? static_cast<const FRawDistributionFloat*>(Value)->GetType()
		: static_cast<const FRawDistributionVector*>(Value)->GetType();
	return Type == ERawDistributionType::Curve;
}

bool FParticleSystemEditor::HasDistributions(const UParticleModule* Module)
{
	bool bFound = false;
	if (Module) ForEachDistribution(Module, [&](const FStructProperty*) { bFound = true; });
	return bFound;
}

bool FParticleSystemEditor::HasCurveTracks(const UParticleModule* Module) const
{
	for (const FCurveTrack& Track : CurveTracks)
		if (Track.Module == Module) return true;
	return false;
}

void FParticleSystemEditor::ToggleCurveTracks(UParticleModule* Module)
{
	if (HasCurveTracks(Module))
	{
		RemoveCurveTracks(Module);
		return;
	}

	int32 Added = 0;
	ForEachDistribution(Module, [&](const FStructProperty* Property)
		{
			FCurveTrack Track;
			Track.Module = Module;
			Track.Property = Property;
			Track.Label = Property->Name;
			Track.NumChannels = IsVectorDistribution(Property) ? 3 : 1;
			if (!Track.IsCurveType()) return;      // Cascade처럼 커브 타입만 올린다

			void* Value = Property->ContainerPtrToValuePtr(Module);
			if (Track.NumChannels == 1) static_cast<FRawDistributionFloat*>(Value)->EnsureCurveKeys();
			else static_cast<FRawDistributionVector*>(Value)->EnsureCurveKeys();

			CurveTracks.Add(Track);
			++Added;
		});

	if (Added == 0)
	{
		HTR_LOG(Info, "No Curve-type distribution in {}. Set Distribution Type to Curve in Details first.", Module->GetClass()->Name);
	}
}

void FParticleSystemEditor::RemoveCurveTracks(const UParticleModule* Module)
{
	for (int32 i = static_cast<int32>(CurveTracks.Num()) - 1; i >= 0; --i)
		if (CurveTracks[i].Module == Module) CurveTracks.RemoveAt(i);
}