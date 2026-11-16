#pragma once

#include "Core/Delegate/Delegate.h"
#include "Editor/EditorUI/EditorTab.h"
#include "Editor/LevelEditor/LevelEditorToolbar.h"
#include "Runtime/Asset/FbxImporter/FbxMeshBake.h"

class FContentDrawerPanel;
class FOutputLogPanel;

class FLevelEditorTab : public IEditorTab
{
public:
	explicit FLevelEditorTab();

	const char* GetTabName() const override { return TabName.c_str(); }
	bool CanDetach() const override { return false; }
	void DrawMenuBar() override;

	TDelegate<> OnNewScene;
	TDelegate<> OnOpenScene;
	TDelegate<> OnSaveScene;
	TDelegate<> OnSaveSceneAs;
	TDelegate<EFbxImportMode> OnImportFbx;
	TDelegate<> OnPackageGame;
	TDelegate<const FString&> OnLevelLoaded;

	void SetIsPackagingQuery(std::function<bool()> InQuery) { IsPackaging = std::move(InQuery); }

	FLevelEditorToolbar* GetToolbar() { return &Toolbar; }
	TArray<TUniquePtr<IEditorPanel>>& GetPanels() { return Panels; }

	template <typename T>
	T* AddPanel()
	{
		TUniquePtr<T> Panel = MakeUnique<T>();
		T* Result = Panel.get();
		Result->Init();
		Panels.Add(std::move(Panel));
		return Result;
	}

	void Tick(float DeltaTime);

	void OnRender(bool bActive) override;
	
	void SetDockSpaceOptions(bool bEnabled, bool bPassthru);

	void UpdateTabName(std::string_view InPath);

	// 하단 상태 표시줄의 Content Drawer / Output Log / Cmd가 쓰는 패널.
	// 툴바의 Browse to Level 버튼도 같은 Content Drawer에 연결한다.
	void SetStatusBarPanels(FContentDrawerPanel* InContentDrawer, FOutputLogPanel* InOutputLog);

	void ToggleContentDrawer();

	void RequestResetLayout() { bResetLayoutRequested = true; }

private:
	// UE5 기본 배치: 가운데 뷰포트, 오른쪽 위 Outliner, 오른쪽 아래 Details, 아래 Output Log.
	void BuildDefaultLayout(ImGuiID DockSpaceId, const ImVec2& Size);
	void DrawStatusBar();
	float GetStatusBarHeight() const;

	FContentDrawerPanel* ContentDrawer = nullptr;
	FOutputLogPanel* OutputLog = nullptr;
	char CommandBuffer[256] = {};
	bool bResetLayoutRequested = false;

	std::function<bool()> IsPackaging;
	FLevelEditorToolbar Toolbar;
	TArray<TUniquePtr<IEditorPanel>> Panels;
	bool bUseDockSpace = true;
	bool bPassthruCentralNode = false;

	FString TabName;
};
