#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/BlendSpaceEditor/BlendSpaceAssetCreator.h"
#include "Editor/AnimBlueprintEditor/AnimBlueprintAssetCreator.h"

#include <filesystem>
#include <functional>
#include <vector>

namespace fs = std::filesystem;

enum class EFbxImportMode : uint32;

// UE5 Content Browser 모양의 콘텐츠 드로어.
// 왼쪽: Favorites / Content 폴더 트리, 오른쪽 위: Add·Import·Save All, 뒤로/앞으로, 경로, Dock 버튼,
// 그 아래: 필터·검색·정렬, 가운데: 에셋 카드 그리드, 맨 아래: 항목 수.
class FContentDrawerPanel : public IEditorPanel
{
public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Content Drawer"; }

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
	// 툴바 Import / Save All 버튼이 부른다.
	void SetImportCallback(std::function<void(EFbxImportMode)> InCallback) { OnImportFbx = std::move(InCallback); }
	void SetSaveAllCallback(std::function<void()> InCallback) { OnSaveAll = std::move(InCallback); }
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
	void RenderContents();

	void DrawSidebar();
	void DrawFolderTree(const fs::path& Path);
	void DrawFolderSearchResults(const fs::path& Path);
	bool DrawSidebarSection(const char* Label, bool& bOpen, bool* bSearchToggle);
	void DrawToolbar();
	void DrawSearchRow();
	void DrawAssetGrid();
	void DrawCreatePopups();

	// 폴더 이동. 뒤로/앞으로 기록을 남긴다.
	void NavigateTo(const fs::path& Path);
	void NavigateBack();
	void NavigateForward();
	bool IsFavorite(const fs::path& Path) const;
	void ToggleFavorite(const fs::path& Path);

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
	std::function<void(EFbxImportMode)> OnImportFbx;
	std::function<void()> OnSaveAll;

	// 만들기 창 (메뉴 팝업 안에서 바로 다른 팝업을 열 수 없어 다음 프레임에 창 기준으로 연다)
	char NewAssetName[64] = "NewWidget";
	bool bOpenCreateWidget = false;
	bool bOpenCreateParticle = false;
	bool bOpenNewFolder = false;

	fs::path AssetRootPath = "Assets";
	fs::path SceneRootPath = "Scenes";
	fs::path CurrentPath = "Assets";
	std::vector<fs::path> BackHistory;
	std::vector<fs::path> ForwardHistory;
	std::vector<fs::path> FavoritePaths;   // 이번 실행 동안만 기억한다
	bool bRevealCurrentFolder = true;      // 이동한 직후 한 번, 트리에서 지금 폴더의 조상 노드를 연다

	// 선택(테두리) 표시할 파일. 그리드 항목 경로와 같은 형식("Scenes/A.scene").
	FString HighlightPath;
	bool bScrollToHighlight = false;

	// 보기 설정
	enum class ESortMode : uint8 { NameAscending, NameDescending, Type };
	char SearchBuffer[128] = {};
	char FolderSearchBuffer[128] = {};
	bool bShowFolderSearch = false;
	bool bFavoritesOpen = true;
	bool bContentOpen = true;
	bool bShowFolders = true;
	uint32 TypeFilterMask = 0;          // 0이면 모든 종류
	ESortMode SortMode = ESortMode::NameAscending;
	float SidebarWidth = 220.0f;        // DPI 배율 1 기준
	float ThumbnailSize = 88.0f;        // DPI 배율 1 기준
	int32 VisibleItemCount = 0;
	bool bSelectionVisible = false;

	FBlendSpaceAssetCreator BlendSpaceCreator;
    FAnimBlueprintAssetCreator AnimBlueprintCreator;
};
