#pragma once

#include "Editor/AssetEditor/AssetEditor.h"
#include "Camera/ViewportCameraMovement.h"
#include <functional>

class UParticleSystem;
class UParticleLOdLevel;
class UParticleModule;
class UParticleSystemComponent;
class UClass;
class UObject;

class FParticleSystemEditor : public FAssetEditor
{
public:
	FParticleSystemEditor(UEditorEngine& InEditorEngine, FEditorManager& InEditorManager, const FString& InAssetPath, UParticleSystem* InSystem);

	// FAssetEditor;
	void Tick(float DeltaTime) override;
	void OnRenderPreview() override;
	const char* GetEditorTypeName()const override { return "ParticleSystem"; }
	bool IsDirty() const override { return bDirty; }
	bool ConfirmClose() override;

	void SetViewportState(uint32 Width, uint32 Height, const FViewportCameraInput& Input);
	void FocusPreview();
	void RestartPreview();
	bool IsShowingTestData() const;
	void SetShowTestData(bool bShow);

	bool bShowBounds = false;
	bool bShowOriginAxis = true;
	void OnRenderLine(FLineBatcher& LineBatcher) override;
	int32 GetActiveParticles(int32 EmitterIndex) const;
	int32 GetMaxActiveParticles(int32 EmitterIndex) const;

	UParticleSystem* GetSystem() const { return System; }






private:
	void ApplyPendingEdits();
	void ResetComponentsUsingSystem();
	void ReinitComponentsUsingSystem();
	void RevertFromDisk();
	FString GetFilePath() const;

	UParticleSystem* System = nullptr;
	UParticleSystemComponent* PreviewScene = nullptr;

	TArray<std::function<void()>> PendingEdits;
	bool bRestartPending = false;
	bool bDirty = false;

	UObject* SelectedObject = nullptr;
	int32 SelectedEmitter = -1;

	FViewportCameraInput CameraInput;
	uint32 ViewportWIdth = 0;
	uint32 ViewportHeight = 0;
	bool bViewportVisisble = false;
	bool bInitialFocusPending = true;
};