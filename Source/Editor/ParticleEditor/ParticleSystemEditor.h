#pragma once

#include "Editor/AssetEditor/AssetEditor.h"
#include "Camera/ViewportCameraMovement.h"
#include "Particles/ParticleCurve.h"
#include <functional>

class UParticleSystem;
class UParticleLODLevel;
class UParticleModule;
class UParticleSystemComponent;
class UClass;
class UObject;
class FStructProperty;

class FParticleSystemEditor : public FAssetEditor
{
public:
	FParticleSystemEditor(UEditorEngine& InEditorEngine, FEditorManager& InEditorManager, const FString& InAssetPath, UParticleSystem* InSystem);

	// ── FAssetEditor ── 
	void Tick(float DeltaTime) override;
	void OnRenderPreview() override;
	const char* GetEditorTypeName()const override { return "ParticleSystem"; }
	bool IsDirty() const override { return bDirty; }
	bool ConfirmClose() override;

	// ── 미리보기 ──
	void SetViewportState(uint32 Width, uint32 Height, const FViewportCameraInput& Input);
	void FocusPreview();
	void RestartPreview();
	bool IsShowingTestData() const;
	void SetShowTestData(bool bShow);

	// ── 툴바 토글과 뷰포트 표시 (Cascade의 Bounds, Origin Axis, 개수 오버레이) ──
	bool bShowBounds = false;
	bool bShowOriginAxis = true;
	void OnRenderLine(FLineBatcher& LineBatcher) override;
	int32 GetActiveParticles(int32 EmitterIndex) const;
	int32 GetMaxActiveParticles(int32 EmitterIndex) const;

	// ── 데이터 · 선택 ──
	UParticleSystem* GetSystem() const { return System; }
	UParticleLODLevel* GetLOD(int32 EmitterIndex) const;
	UObject* GetSelectedObject() const { return SelectedObject; }
	void Select(int32 EmitterIndex, UObject* Object);

	// ── 편집 명령: 큐에 넣고 다음 Tick에 적용 ──
	void RequestAddEmitter();
	void RequestRemoveEmitter(int32 EmitterIndex);
	void RequestAddModule(int32 EmitterIndex, UClass* ModuleClass);
	void RequestRemoveModule(int32 EmitterIndex, UParticleModule* Module);
	void RequestMoveModule(int32 EmitterIndex, int32 From, int32 To);
	void RequestSetTypeData(int32 EmitterIndex, UClass* TypeDataClass);   // nullptr = Sprite
	void RequestDeleteSelected();
	void NotifyPropertyChanged();                // 값만 바뀜

	bool Save();
	void HandleShortcuts();                      // 패널의 DrawContent 안에서 부른다

	// ── 커브 에디터 트랙 (Cascade: 모듈의 커브 버튼으로 올린 Distribution) ──
	struct FCurveTrack
	{
		UParticleModule* Module = nullptr;
		const FStructProperty* Property = nullptr;   // FRawDistributionFloat 또는 FRawDistributionVector 멤버
		FString Label;                               // "AlphaOverLife"
		int32 NumChannels = 1;                       // Float 1, Vector 3
		bool bChannelVisible[3] = { true, true, true };

		FParticleCurve* GetChannel(int32 Channel) const;
		bool IsCurveType() const;                    // 디테일에서 Type을 다른 걸로 바꿨으면 false
	};

	TArray<FCurveTrack>& GetCurveTracks() { return CurveTracks; }
	static bool HasDistributions(const UParticleModule* Module);
	bool HasCurveTracks(const UParticleModule* Module) const;
	void ToggleCurveTracks(UParticleModule* Module);       // 모듈 줄의 커브 버튼
	void RemoveCurveTracks(const UParticleModule* Module);  // 모듈을 지우기 전에

protected:
	TUniquePtr<IEditorTab> CreateTab() override;
	bool InitializeResources() override;
	void ShutdownResources() override;

private:
	void ApplyPendingEdits();
	void ResetComponentsUsingSystem();
	void ReinitComponentsUsingSystem();
	void RevertFromDisk();
	FString GetFilePath() const;

	UParticleSystem* System = nullptr;
	UParticleSystemComponent* PreviewComponent = nullptr;

	TArray<std::function<void()>> PendingEdits;
	bool bRestartPending = false;
	bool bDirty = false;

	UObject* SelectedObject = nullptr;
	int32 SelectedEmitter = -1;

	FViewportCameraInput CameraInput;
	uint32 ViewportWidth = 0;
	uint32 ViewportHeight = 0;
	bool bViewportVisible = false;
	bool bInitialFocusPending = true;

	TArray<FCurveTrack> CurveTracks;
};