#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/BlendSpaceEditor/BlendSpaceAssetCreator.h"
#include "Editor/AnimBlueprintEditor/AnimBlueprintAssetCreator.h"

#include <filesystem>
#include <functional>

namespace fs = std::filesystem;

class FContentDrawerPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Content Drawer"; }
	void RenderFolderTree(const fs::path& Path, fs::path& SelectedPath);

	// UE5처럼 상태 표시줄 위로 올라오는 드로어로 쓴다. "Dock in Layout"을 누르면 일반 도킹 창이 된다.
	void ToggleDrawer();
	// 드로어를 열고 파일이 있는 폴더로 이동해 그 파일을 선택 표시한다. 파일이 없으면 false.
	bool BrowseToFile(const fs::path& FilePath);
	bool IsDrawerOpen() const { return bDrawerOpen; }
	bool IsDocked() const { return bDocked; }
	// 드로어가 덮을 영역 (레벨 에디터 탭이 매 프레임 넘긴다).
	void SetDrawerRect(const ImVec2& Pos, const ImVec2& Size, ImGuiID ViewportId);

	// .widget 파일을 더블클릭하면 불린다 (UI 에디터가 연다).
	void SetOpenWidgetCallback(std::function<void(const FString&)> InCallback) { OnOpenWidget = std::move(InCallback); }
	void SetCreateWidgetCallback(std::function<void(const FString&)> InCallback) { OnCreateWidget = std::move(InCallback); }
	// OBJ 또는 베이크된 FBX 메시를 더블클릭하면 Viewer를 엽니다.
	void SetOpenMeshCallback(std::function<void(const FString&)> InCallback) { OnOpenMesh = std::move(InCallback); }
	void SetBlendSpaceCreatedCallback(std::function<void(const FString&)> InCallback)
	{
		BlendSpaceCreator.SetCreatedCallback(std::move(InCallback));
	}

    void SetAnimBlueprintCreatedCallback(std::function<void(const FString&)> InCallback)
    {
        AnimBlueprintCreator.SetCreatedCallback(std::move(InCallback));
    }

private:
	bool BeginWindow();
	void DrawHeader();
	void RenderContents();

	bool bDocked = false;
	bool bDrawerOpen = false;
	bool bJustOpened = false;
	bool bPendingDock = false;
	ImVec2 DrawerPos = ImVec2(0.0f, 0.0f);
	ImVec2 DrawerSize = ImVec2(0.0f, 0.0f);
	ImGuiID DrawerViewportId = 0;

	std::function<void(const FString&)> OnOpenWidget;
	std::function<void(const FString&)> OnCreateWidget;
	std::function<void(const FString&)> OnOpenMesh;
	char NewAssetName[64] = "NewWidget";
	bool bOpenCreateWidget = false;

	fs::path AssetRootPath = "Assets";
	fs::path SceneRootPath = "Scenes";
	fs::path CurrentPath = "Assets";
	// 선택(테두리) 표시할 파일. 그리드 항목 경로와 같은 형식("Scenes/A.scene").
	FString HighlightPath;
	bool bScrollToHighlight = false;

	const FString FolderIconPath = "Engine/Icons/Folder.png";
	const FString FileIconPath = "Engine/Icons/File.png";

	FBlendSpaceAssetCreator BlendSpaceCreator;
    FAnimBlueprintAssetCreator AnimBlueprintCreator;
};
