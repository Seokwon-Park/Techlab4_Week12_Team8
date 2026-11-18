#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/EditorUI/EditorCommand.h"
#include "Editor/EditorUI/EditorIcons.h"
#include "Editor/HitoriEd/PlayInEditorDataTypes.h"

class FEditorModeTools;

// 툴바 드롭다운 메뉴 한 줄. Category가 바뀌는 곳마다 대문자 섹션 제목을 넣는다.
struct FToolbarMenuEntry
{
	const char* Category = nullptr;
	const char* Label = nullptr;
	EEditorIcon Icon = EEditorIcon::None;
	std::function<void()> Execute;
	std::function<bool()> CanExecute;
};

// UE5처럼 "아이콘 ▾"로 보이는 툴바 드롭다운. bShowLabel이면 아이콘 옆에 글자도 쓴다.
struct FToolbarMenu
{
	EEditorIcon Icon = EEditorIcon::None;
	const char* Label = nullptr;
	const char* Tooltip = nullptr;
	bool bShowLabel = false;
	TArray<FToolbarMenuEntry> Entries;
};

// UE5 레벨 에디터 툴바 순서:
// [Save][Browse]   [Selection Mode ▾]   [Add ▾][Cinematics ▾]   ( ▶ |▶ ■ ⋮ )
class FLevelEditorToolbar
{
public:
	~FLevelEditorToolbar() = default;

	void SetSaveCommand(const FEditorCommand& InSave) { SaveCommand = InSave; bHasSaveCommand = true; }
	// Save 옆 "Browse to Level" 버튼. 현재 레벨 파일을 Content Drawer에서 찾아 선택한다.
	// 레벨을 아직 저장하지 않아 파일이 없으면 CanBrowse가 false라 버튼이 꺼진다.
	void SetBrowseToLevelAction(std::function<void()> InAction, std::function<bool()> InCanBrowse)
	{
		BrowseToLevel = std::move(InAction);
		CanBrowseToLevel = std::move(InCanBrowse);
	}
	void SetCommands(const TArray<FEditorCommand>& InCommands) { Commands = InCommands; }
	// Selection Mode 드롭다운이 읽고 바꾸는 모드 툴 (레벨 에디터면 GLevelEditorModeTools()).
	void SetModeTools(FEditorModeTools* InModeTools) { ModeTools = InModeTools; }
	// Save 오른쪽, Play 그룹 왼쪽에 놓이는 메뉴.
	void SetCreateMenus(const TArray<FToolbarMenu>& InMenus) { CreateMenus = InMenus; }
	// 툴바 오른쪽 끝에 붙는 메뉴.
	void SetRightMenus(const TArray<FToolbarMenu>& InMenus) { RightMenus = InMenus; }
	// Play 그룹 끝 ⋮ 메뉴에서 PIE 실행 위치를 고른다. 엔진이 상태를 소유한다.
	void SetPlayModeBinding(std::function<EPlayModeType()> InGetPlayMode,
		std::function<void(EPlayModeType)> InSetPlayMode, std::function<bool()> InCanChangePlayMode)
	{
		GetPlayMode = std::move(InGetPlayMode);
		SetPlayMode = std::move(InSetPlayMode);
		CanChangePlayMode = std::move(InCanChangePlayMode);
	}
	void Draw();

	static constexpr float Height = 40.0f;

private:
	void DrawFileGroup();
	void DrawSelectionModeMenu();
	void DrawCreateGroup();
	void DrawPlayGroup();
	void DrawPlayModeMenu();
	void DrawRightGroup();

	static float CalcMenuButtonWidth(const FToolbarMenu& Menu);
	static void DrawMenuButton(const FToolbarMenu& Menu);

	FEditorCommand SaveCommand{};
	bool bHasSaveCommand = false;
	FEditorModeTools* ModeTools = nullptr;
	std::function<void()> BrowseToLevel;
	std::function<bool()> CanBrowseToLevel;
	TArray<FEditorCommand> Commands;
	TArray<FToolbarMenu> CreateMenus;
	TArray<FToolbarMenu> RightMenus;
	std::function<EPlayModeType()> GetPlayMode;
	std::function<void(EPlayModeType)> SetPlayMode;
	std::function<bool()> CanChangePlayMode;
};
