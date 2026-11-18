#pragma once

#include "Editor/AssetEditor/AssetEditorPanel.h"

class FParticleSystemEditor;
class UParticleEmitter;
class UParticleLODLevel;
class UParticleModule;
class UClass;

// 에디터 타입별 패널 베이스는 별칭만 둔다 (FUIEditorPanel과 같은 방식)
using FParticleEditorPanel = TAssetEditorPanel<FParticleSystemEditor>;
inline constexpr const char* ParticlePanelIdPrefix = "ParticleEditorPanel";

class FParticleViewportPanel : public FParticleEditorPanel
{
public:
	explicit FParticleViewportPanel(FParticleSystemEditor& InEditor);
protected:
	void DrawContent() override;
	ImGuiWindowFlags GetWindowFlags() const override { return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse; }
};

class FParticleEmittersPanel : public FParticleEditorPanel
{
public:
	explicit FParticleEmittersPanel(FParticleSystemEditor& InEditor);
protected:
	void DrawContent() override;
private:
	void DrawEmitterColumn(int32 EmitterIndex);
	void DrawEmitterHeader(int32 EmitterIndex, UParticleEmitter* Emitter, UParticleLODLevel* LOD);
	void DrawModuleRow(int32 EmitterIndex, UParticleLODLevel* LOD, UParticleModule* Module, int32 ModuleIndex);
	void DrawAddModuleMenu(int32 EmitterIndex, UParticleLODLevel* LOD);
};

class FParticleDetailsPanel : public FParticleEditorPanel
{
public:
	explicit FParticleDetailsPanel(FParticleSystemEditor& InEditor);
protected:
	void DrawContent() override;
};

class FParticleCurveEditorPanel : public FParticleEditorPanel
{
public:
	explicit FParticleCurveEditorPanel(FParticleSystemEditor& InEditor);
protected:
	void DrawContent() override;
	ImGuiWindowFlags GetWindowFlags() const override { return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse; }
private:
	void DrawToolbar();
	void DrawTrackList();
	void DrawGraph();
	void FitHorizontal();
	void FitVertical();
	struct FParticleCurve* GetSelectedCurve() const;

	// 보이는 범위 (시간, 값)
	float ViewMinT = -0.05f, ViewMaxT = 1.05f;
	float ViewMinV = -0.1f, ViewMaxV = 1.1f;

	// 선택: 트랙 · 채널 · 키
	int32 SelectedTrack = -1;
	int32 SelectedChannel = 0;
	int32 SelectedKey = -1;
	bool bDraggingKey = false;

	// 툴바 Pan / Zoom: 빈 곳을 왼쪽으로 끌 때 할 일
	bool bZoomMode = false;
	bool bDraggingView = false;
	float ViewDragAnchorX = 0.0f, ViewDragAnchorY = 0.0f;   // Zoom 고정점 (화면 좌표)
};