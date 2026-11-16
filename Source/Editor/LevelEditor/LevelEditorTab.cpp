#include "EnginePCH.h"
#include "Editor/LevelEditor/LevelEditorTab.h"
#include "Editor/HitoriEd/EditorFileUtils.h"
#include "Asset/FbxImporter/FbxMeshBake.h"
#include "Editor/ContentDrawer/ContentDrawerPanel.h"
#include "Editor/OutputLog/OutputLogPanel.h"
#include "Editor/EditorUI/EditorIcons.h"
#include <filesystem>

namespace
{
	// 켜져 있으면 선택 행 색으로 칠하고, 꺼져 있으면 배경 없이 아이콘+글자만 보이는 상태 표시줄 버튼.
	// 마우스를 누르는 순간 반응해야 드로어의 "포커스 잃으면 닫기"와 엇갈리지 않는다.
	bool StatusBarToggle(const char* Id, EEditorIcon Icon, const char* Label, bool bActive)
	{
		return EditorIcons::Button(Id, Icon, ImGui::GetColorU32(ImGuiCol_Text, 0.8f), Label, false, bActive,
			ImGui::GetFrameHeight(), ImGuiButtonFlags_PressedOnClick);
	}
}

FLevelEditorTab::FLevelEditorTab()
{
	OnLevelLoaded.Add([this](std::string_view Path) {
		FLevelEditorTab::UpdateTabName(Path);
	});

	FLevelEditorTab::UpdateTabName(FEditorFileUtils::CurrentScenePath);
}

void FLevelEditorTab::DrawMenuBar()
{
	////////////////////////////////////////////////////////////
	// File 탭
	////////////////////////////////////////////////////////////

	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("New Scene")) { OnNewScene.Broadcast(); }
		if (ImGui::MenuItem("Open Scene...")) { OnOpenScene.Broadcast(); }

		ImGui::Separator();

		if (ImGui::MenuItem("Save")) { OnSaveScene.Broadcast(); }
		if (ImGui::MenuItem("Save As...")) { OnSaveSceneAs.Broadcast(); }

		ImGui::Separator();

		if (ImGui::BeginMenu("Import FBX"))
		{
			if (ImGui::MenuItem("Static Mesh...")) { OnImportFbx.Broadcast(EFbxImportMode::Static); }
			if (ImGui::MenuItem("Skeletal Mesh...")) { OnImportFbx.Broadcast(EFbxImportMode::Skeletal); }
			ImGui::EndMenu();
		}

		ImGui::EndMenu();
	}



	////////////////////////////////////////////////////////////
	// Window 탭
	////////////////////////////////////////////////////////////

	if (ImGui::BeginMenu("Window"))
	{
		for (auto& Panel : Panels)
		{
			bool bOpen = Panel->IsOpen();

			if (ImGui::MenuItem(Panel->GetPanelName(), nullptr, bOpen))
			{
				Panel->SetOpen(!bOpen);
			}
		}

		ImGui::Separator();
		if (ImGui::MenuItem("Reset Layout")) { RequestResetLayout(); }
		ImGui::EndMenu();
	}



	////////////////////////////////////////////////////////////
	// Build 탭
	////////////////////////////////////////////////////////////

	if (ImGui::BeginMenu("Build"))
	{
		const bool bCanPackage = !IsPackaging || !IsPackaging();
		if (ImGui::MenuItem("Package Game (Release)", nullptr, false, bCanPackage)) { OnPackageGame.Broadcast(); }
		ImGui::EndMenu();
	}
}

void FLevelEditorTab::Tick(float DeltaTime)
{
	for (auto& Panel : Panels)
		Panel->Tick(DeltaTime);
}

void FLevelEditorTab::OnRender(bool bActive)
{
	if (bActive)
		Toolbar.Draw();

	// 상태 표시줄 자리를 먼저 빼고 남은 영역을 도크스페이스가 쓴다.
	const float StatusBarHeight = bActive ? GetStatusBarHeight() : 0.0f;
	const ImVec2 DockAreaPos = ImGui::GetCursorScreenPos();
	const ImVec2 Avail = ImGui::GetContentRegionAvail();
	const ImVec2 DockAreaSize(Avail.x, ImMax(1.0f, Avail.y - StatusBarHeight));

	if (bUseDockSpace && (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable))
	{
		// 부모 창이 달라져도 패널 도킹 ID는 유지한다.
		// UE5 레이아웃으로 바꾸면서 ID를 새로 잡았다. 저장된 노드가 없으면 기본 배치를 한 번 만든다.
		const ImGuiID DockSpaceId = ImHashStr("LevelEditorDockSpace_UE5", 0, ImHashStr("DockingSpace"));
		if (bActive && (bResetLayoutRequested || ImGui::DockBuilderGetNode(DockSpaceId) == nullptr))
		{
			BuildDefaultLayout(DockSpaceId, DockAreaSize);
			bResetLayoutRequested = false;
		}

		ImGuiDockNodeFlags Flags = bPassthruCentralNode
			? ImGuiDockNodeFlags_PassthruCentralNode : ImGuiDockNodeFlags_None;
		if (!bActive)
			Flags |= ImGuiDockNodeFlags_KeepAliveOnly;

		// 패널 탭 높이를 패널 Begin과 같은 값으로 맞춘다 (EditorPanelUI).
		EditorPanelUI::PushTabPadding();
		ImGui::DockSpace(DockSpaceId, ImVec2(0.0f, -StatusBarHeight), Flags);
		EditorPanelUI::PopTabPadding();
	}

	if (bActive)
	{
		ImGui::SetCursorScreenPos(ImVec2(DockAreaPos.x, DockAreaPos.y + DockAreaSize.y));
		DrawStatusBar();

		// 드로어는 상태 표시줄 바로 위에서 도크 영역 아래쪽 40%를 덮는다.
		if (ContentDrawer)
		{
			const float DrawerHeight = ImMin(DockAreaSize.y, ImMax(240.0f, DockAreaSize.y * 0.4f));
			ContentDrawer->SetDrawerRect(
				ImVec2(DockAreaPos.x, DockAreaPos.y + DockAreaSize.y - DrawerHeight),
				ImVec2(DockAreaSize.x, DrawerHeight),
				ImGui::GetWindowViewport()->ID);
		}

		for (auto& Panel : Panels)
		{
			if (Panel->IsOpen())
				Panel->OnRender();
		}
	}
}

void FLevelEditorTab::BuildDefaultLayout(ImGuiID DockSpaceId, const ImVec2& Size)
{
	ImGui::DockBuilderRemoveNode(DockSpaceId);
	ImGui::DockBuilderAddNode(DockSpaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(DockSpaceId, Size);

	ImGuiID Left = DockSpaceId;
	ImGuiID RightTop = ImGui::DockBuilderSplitNode(Left, ImGuiDir_Right, 0.20f, nullptr, &Left);
	ImGuiID RightBottom = ImGui::DockBuilderSplitNode(RightTop, ImGuiDir_Down, 0.65f, nullptr, &RightTop);
	ImGuiID Center = Left;
	ImGuiID Bottom = ImGui::DockBuilderSplitNode(Center, ImGuiDir_Down, 0.22f, nullptr, &Center);

	ImGui::DockBuilderDockWindow("Viewports", Center);
	ImGui::DockBuilderDockWindow("Outliner", RightTop);
	ImGui::DockBuilderDockWindow("Details", RightBottom);
	ImGui::DockBuilderDockWindow("Settings", RightBottom);
	ImGui::DockBuilderDockWindow("Editor Controls", RightBottom);
	ImGui::DockBuilderDockWindow("Output Log", Bottom);
	ImGui::DockBuilderDockWindow("Sequencer", Bottom);
	// 드로어를 "Dock in Layout"으로 붙였을 때만 쓰는 창 이름이다.
	ImGui::DockBuilderDockWindow("Content Drawer", Bottom);

	// 뷰포트는 탭 없이 꽉 차게 (탭 줄은 노드 왼쪽 위 삼각형으로 다시 펼 수 있다).
	if (ImGuiDockNode* CenterNode = ImGui::DockBuilderGetNode(Center))
		CenterNode->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;

	ImGui::DockBuilderFinish(DockSpaceId);
}

void FLevelEditorTab::SetStatusBarPanels(FContentDrawerPanel* InContentDrawer, FOutputLogPanel* InOutputLog)
{
	ContentDrawer = InContentDrawer;
	OutputLog = InOutputLog;

	// UE5 "Browse to Level": 저장된 레벨 파일이 있을 때만 켜진다.
	Toolbar.SetBrowseToLevelAction(
		[this]()
		{
			if (ContentDrawer)
				ContentDrawer->BrowseToFile(FEditorFileUtils::CurrentScenePath);
		},
		[this]()
		{
			std::error_code Error;
			return ContentDrawer && !FEditorFileUtils::CurrentScenePath.empty()
				&& std::filesystem::is_regular_file(FEditorFileUtils::CurrentScenePath, Error);
		});
}

void FLevelEditorTab::ToggleContentDrawer()
{
	if (!ContentDrawer)
		return;
	ContentDrawer->SetOpen(true);
	ContentDrawer->ToggleDrawer();
}

float FLevelEditorTab::GetStatusBarHeight() const
{
	return ImGui::GetFrameHeight() + 6.0f;
}

void FLevelEditorTab::DrawStatusBar()
{
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Space, ImGuiInputFlags_RouteGlobal))
		ToggleContentDrawer();

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 3.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));
	const bool bVisible = ImGui::BeginChild("##LevelEditorStatusBar", ImVec2(0.0f, GetStatusBarHeight()),
		ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	if (bVisible)
	{
		if (ContentDrawer)
		{
			const bool bActive = ContentDrawer->IsDrawerOpen() || ContentDrawer->IsDocked();
			if (StatusBarToggle("##StatusContentDrawer", EEditorIcon::Folder, "Content Drawer", bActive))
			{
				ToggleContentDrawer();
			}
			ImGui::SetItemTooltip("Ctrl+Space");
			ImGui::SameLine();
		}

		if (OutputLog)
		{
			if (StatusBarToggle("##StatusOutputLog", EEditorIcon::None, "Output Log", OutputLog->IsOpen()))
				OutputLog->SetOpen(!OutputLog->IsOpen());
			ImGui::SameLine();

			// 구분선
			const ImVec2 P = ImGui::GetCursorScreenPos();
			const float H = ImGui::GetFrameHeight();
			ImGui::GetWindowDrawList()->AddLine(ImVec2(P.x + 2.0f, P.y + 3.0f), ImVec2(P.x + 2.0f, P.y + H - 3.0f),
				ImGui::GetColorU32(ImGuiCol_Separator));
			ImGui::Dummy(ImVec2(5.0f, H));
			ImGui::SameLine();

			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled("Cmd");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImMin(320.0f, ImGui::GetContentRegionAvail().x));
			if (ImGui::InputTextWithHint("##StatusBarCmd", "Enter Console Command", CommandBuffer, IM_COUNTOF(CommandBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll))
			{
				if (CommandBuffer[0] != '\0')
					OutputLog->ExecCommand(CommandBuffer);
				CommandBuffer[0] = '\0';
				ImGui::SetKeyboardFocusHere(-1);
			}
		}
	}

	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
}

void FLevelEditorTab::SetDockSpaceOptions(bool bEnabled, bool bPassthru)
{
	bUseDockSpace = bEnabled;
	bPassthruCentralNode = bPassthru;
}

void FLevelEditorTab::UpdateTabName(std::string_view InPath)
{
	const FString LevelName = InPath.empty()
		? "EmptyLevel" : std::filesystem::path(InPath).filename().string();
	TabName = LevelName + " (LevelEditor)###LevelEditor";
}
