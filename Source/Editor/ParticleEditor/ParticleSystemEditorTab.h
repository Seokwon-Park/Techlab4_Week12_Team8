#pragma once

#include "Editor/AssetEditor/DockedAssetEditorTab.h"

class FParticleSystemEditor;

class FParticleSystemEditorTab : public FDockedAssetEditorTab
{
public:
	explicit FParticleSystemEditorTab(FParticleSystemEditor& InEditor);
	void OnRender(bool bActive) override;        // 툴바 → 독스페이스

protected:
	void BeforeRender() override;
	void BuildDefaultLayout(ImGuiID InDockSpaceId, const ImVec2& Size) override;

private:
	void DrawToolbar();
	FParticleSystemEditor& ParticleEditor;
};