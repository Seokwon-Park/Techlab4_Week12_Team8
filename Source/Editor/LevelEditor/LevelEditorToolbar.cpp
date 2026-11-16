#include "EnginePCH.h"
#include "LevelEditorToolbar.h"

#include "Asset/AssetManager.h"
#include "Editor/EditorModeManager.h"

namespace
{
	// UE5 툴바 아이콘 기본색 (밝은 회색)
	ImU32 IconGray() { return ImGui::GetColorU32(ImGuiCol_Text, 0.78f); }

	void BeginToolbarGroup()
	{
		ImDrawList* DL = ImGui::GetWindowDrawList();
		DL->ChannelsSplit(2);
		DL->ChannelsSetCurrent(1);
		ImGui::BeginGroup();
	}

	// 그룹 뒤에 입력 칸과 같은 어두운 알약 모양 배경을 깐다 (UE5 Play 그룹).
	void EndToolbarGroup()
	{
		ImGui::EndGroup();
		const float S = EditorIcons::GetScale();
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();

		ImDrawList* DL = ImGui::GetWindowDrawList();
		DL->ChannelsSetCurrent(0);
		DL->AddRectFilled({ Min.x - 4 * S, Min.y - 2 * S }, { Max.x + 4 * S, Max.y + 2 * S },
			ImGui::GetColorU32(ImGuiCol_FrameBg), 5.0f * S);
		DL->ChannelsMerge();
	}

	// UE5는 그룹 사이를 선 없이 넓은 간격으로만 나눈다.
	void GroupGap()
	{
		ImGui::SameLine(0.0f, 18.0f * EditorIcons::GetScale());
	}
}

void FLevelEditorToolbar::Draw()
{
	const float S = EditorIcons::GetScale();
	const float ButtonHeight = 28.0f * S;
	const float BarHeight = ImMax(Height, ButtonHeight + 12.0f * S);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8 * S, 0 });
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 2 * S, 4 * S });
	const bool bOpen = ImGui::BeginChild("##LevelEditorToolbar", ImVec2(0.0f, BarHeight),
		ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	if (bOpen)
	{
		ImGui::SetCursorPosY((BarHeight - ButtonHeight) * 0.5f);   // 세로 가운데 정렬

		DrawFileGroup();
		if (ModeTools)
		{
			GroupGap();
			DrawSelectionModeMenu();
		}
		if (!CreateMenus.IsEmpty())
		{
			GroupGap();
			DrawCreateGroup();
		}
		GroupGap();
		DrawPlayGroup();
		DrawRightGroup();
	}
	ImGui::EndChild();                  // Begin 결과와 관계없이 항상 EndChild
	ImGui::PopStyleVar(2);
}

void FLevelEditorToolbar::DrawFileGroup()
{
	if (bHasSaveCommand)
	{
		FEditorCommand Save = SaveCommand;
		Save.IconTint = ImGui::ColorConvertU32ToFloat4(IconGray());
		EditorCommandUI::ToolbarButton("##Save", Save);
	}
	if (BrowseToLevel)
	{
		ImGui::SameLine();
		const bool bCanBrowse = !CanBrowseToLevel || CanBrowseToLevel();
		ImGui::BeginDisabled(!bCanBrowse);
		// 드로어가 "포커스 잃으면 닫기"를 하므로 누르는 순간 실행해야 열리자마자 닫히지 않는다.
		const bool bPressed = EditorIcons::Button("##BrowseToLevel", EEditorIcon::BrowseContent, IconGray(), nullptr,
			false, false, 0.0f, ImGuiButtonFlags_PressedOnClick);
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_ForTooltip))
		{
			ImGui::SetTooltip("%s", bCanBrowse
				? "Browses to the current level and selects it in the Content Drawer"
				: "The current level has not been saved yet, so there is no level file to browse to");
		}
		if (bPressed)
			BrowseToLevel();
	}
}

// UE5 "Selection Mode ▾": 어두운 알약 안에 현재 편집 모드를 보여 주고, 누르면 모드와 기즈모 설정 메뉴를 연다.
void FLevelEditorToolbar::DrawSelectionModeMenu()
{
	const float S = EditorIcons::GetScale();
	const ImVec2 Min = ImGui::GetCursorScreenPos();

	BeginToolbarGroup();
	const bool bPopupOpen = ImGui::IsPopupOpen("##SelectionModeMenu");
	const bool bClicked = EditorIcons::Button("##SelectionMode", EEditorIcon::SelectMode,
		ImGui::GetColorU32(ImGuiCol_Text, 0.9f), "Selection Mode", true, bPopupOpen);
	EndToolbarGroup();
	ImGui::SetItemTooltip("Change the editor mode and transform tool settings");
	if (bClicked)
		ImGui::OpenPopup("##SelectionModeMenu");

	ImGui::SetNextWindowPos(ImVec2(Min.x, Min.y + 30.0f * S), ImGuiCond_Appearing);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f * S, 6.0f * S));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * S, 1.0f * S));
	if (ImGui::BeginPopup("##SelectionModeMenu"))
	{
		EditorIcons::MenuSection("Modes");
		if (EditorIcons::MenuRow("Selection", EEditorIcon::SelectMode, ModeTools->IsModeActive(EEditorModeID::Selection)))
			ModeTools->ActivateMode(EEditorModeID::Selection);

		ImGui::Spacing();
		EditorIcons::MenuSection("Transform");
		const UE::Widget::EWidgetMode WidgetMode = ModeTools->GetWidgetMode();
		if (EditorIcons::MenuRow("Translate", EEditorIcon::Translate, WidgetMode == UE::Widget::WM_Translate))
			ModeTools->SetWidgetMode(UE::Widget::WM_Translate);
		if (EditorIcons::MenuRow("Rotate", EEditorIcon::Rotate, WidgetMode == UE::Widget::WM_Rotate))
			ModeTools->SetWidgetMode(UE::Widget::WM_Rotate);
		if (EditorIcons::MenuRow("Scale", EEditorIcon::Scale, WidgetMode == UE::Widget::WM_Scale))
			ModeTools->SetWidgetMode(UE::Widget::WM_Scale);

		ImGui::Spacing();
		EditorIcons::MenuSection("Coordinate Space");
		const ECoordSystem Coord = ModeTools->GetCoordSystem();
		if (EditorIcons::MenuRow("World", EEditorIcon::World, Coord == COORD_World))
			ModeTools->SetCoordSystem(COORD_World);
		if (EditorIcons::MenuRow("Local", EEditorIcon::Local, Coord == COORD_Local))
			ModeTools->SetCoordSystem(COORD_Local);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		EditorIcons::MenuNote("Press Space in the viewport to cycle Translate, Rotate and Scale.");
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);
}

void FLevelEditorToolbar::DrawCreateGroup()
{
	bool bFirst = true;
	for (const FToolbarMenu& Menu : CreateMenus)
	{
		if (!bFirst) ImGui::SameLine(0, 4 * EditorIcons::GetScale());
		DrawMenuButton(Menu);
		bFirst = false;
	}
}

void FLevelEditorToolbar::DrawPlayGroup()
{
	BeginToolbarGroup();

	// Play/Pause/Resume은 IsVisible로 셋 중 하나만 그려진다.
	// SameLine은 "그려졌을 때만" 붙여야 빈 간격이 생기지 않는다.
	bool bFirst = true;
	for (const FEditorCommand& Command : Commands)
	{
		if (!Command.IsVisibleNow())
			continue;
		if (!bFirst) ImGui::SameLine();
		EditorCommandUI::ToolbarButton(Command.Label, Command);
		bFirst = false;
	}
	if (!bFirst) ImGui::SameLine();
	DrawPlayModeMenu();

	EndToolbarGroup();
}

void FLevelEditorToolbar::DrawPlayModeMenu()
{
	if (!GetPlayMode || !SetPlayMode)
		return;

	const float S = EditorIcons::GetScale();
	const bool bCanChange = !CanChangePlayMode || CanChangePlayMode();
	const EPlayModeType Current = GetPlayMode();

	// ⋮ 버튼은 아이콘보다 좁게
	const float Width = 18.0f * S;
	const ImVec2 Min = ImGui::GetCursorScreenPos();
	if (ImGui::InvisibleButton("##PlayModeMenu", ImVec2(Width, 28.0f * S)))
		ImGui::OpenPopup("PlayModePopup");
	const bool bHovered = ImGui::IsItemHovered();
	ImDrawList* DL = ImGui::GetWindowDrawList();
	ImU32 Bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
	if (bHovered || ImGui::IsItemActive())
	{
		Bg = ImGui::GetColorU32(ImGui::IsItemActive() ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered);
		DL->AddRectFilled(Min, ImVec2(Min.x + Width, Min.y + 28.0f * S), Bg, 3.0f * S);
	}
	EditorIcons::Draw(DL, EEditorIcon::MoreVertical, ImVec2(Min.x + Width * 0.5f, Min.y + 14.0f * S), 16.0f * S,
		ImGui::GetColorU32(ImGuiCol_Text, 0.85f), Bg);
	ImGui::SetItemTooltip("Change Play Mode and Play Settings");

	ImGui::SetNextWindowPos(ImVec2(Min.x, Min.y + 30.0f * S), ImGuiCond_Appearing);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f * S, 6.0f * S));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * S, 1.0f * S));
	if (ImGui::BeginPopup("PlayModePopup"))
	{
		EditorIcons::MenuSection("Modes");
		if (EditorIcons::MenuRow("Selected Viewport", EEditorIcon::Play, Current == EPlayModeType::InViewport, bCanChange))
			SetPlayMode(EPlayModeType::InViewport);
		if (EditorIcons::MenuRow("New Editor Window (PIE)", EEditorIcon::NewWindow, Current == EPlayModeType::InEditorFloating, bCanChange))
			SetPlayMode(EPlayModeType::InEditorFloating);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		EditorIcons::MenuNote(bCanChange
			? "The mode you pick here becomes the default for the toolbar Play button."
			: "Stop the current play session to change the play mode.");
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);
}

void FLevelEditorToolbar::DrawRightGroup()
{
	if (RightMenus.IsEmpty())
		return;

	const float S = EditorIcons::GetScale();
	float TotalWidth = 0.0f;
	for (const FToolbarMenu& Menu : RightMenus)
		TotalWidth += CalcMenuButtonWidth(Menu) + 4.0f * S;

	ImGui::SameLine();
	const float StartX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - TotalWidth;
	if (StartX > ImGui::GetCursorPosX())
		ImGui::SetCursorPosX(StartX);

	bool bFirst = true;
	for (const FToolbarMenu& Menu : RightMenus)
	{
		if (!bFirst) ImGui::SameLine(0, 4 * S);
		DrawMenuButton(Menu);
		bFirst = false;
	}
}

float FLevelEditorToolbar::CalcMenuButtonWidth(const FToolbarMenu& Menu)
{
	return EditorIcons::CalcButtonWidth(Menu.Icon != EEditorIcon::None, Menu.bShowLabel ? Menu.Label : nullptr, true);
}

void FLevelEditorToolbar::DrawMenuButton(const FToolbarMenu& Menu)
{
	const float S = EditorIcons::GetScale();
	ImGui::PushID(Menu.Label);
	const ImVec2 Min = ImGui::GetCursorScreenPos();
	const bool bPopupOpen = ImGui::IsPopupOpen("##ToolbarMenu");
	const bool bClicked = EditorIcons::Button("##MenuButton", Menu.Icon, IconGray(),
		Menu.bShowLabel ? Menu.Label : nullptr, true, bPopupOpen);
	ImGui::SetItemTooltip("%s", Menu.Tooltip ? Menu.Tooltip : Menu.Label);
	if (bClicked)
		ImGui::OpenPopup("##ToolbarMenu");

	// 팝업은 버튼 바로 아래 왼쪽 정렬로 연다.
	ImGui::SetNextWindowPos(ImVec2(Min.x, Min.y + 30.0f * S), ImGuiCond_Appearing);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f * S, 6.0f * S));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * S, 1.0f * S));
	if (ImGui::BeginPopup("##ToolbarMenu"))
	{
		const char* CurrentCategory = nullptr;
		for (const FToolbarMenuEntry& Entry : Menu.Entries)
		{
			if (Entry.Category && (!CurrentCategory || std::strcmp(CurrentCategory, Entry.Category) != 0))
			{
				if (CurrentCategory)
					ImGui::Spacing();
				EditorIcons::MenuSection(Entry.Category);
				CurrentCategory = Entry.Category;
			}

			const bool bEnabled = !Entry.CanExecute || Entry.CanExecute();
			if (EditorIcons::MenuRow(Entry.Label, Entry.Icon, false, bEnabled) && Entry.Execute)
				Entry.Execute();
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(2);
	ImGui::PopID();
}
