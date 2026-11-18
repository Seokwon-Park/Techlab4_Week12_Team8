#include "EnginePCH.h"
#include "Editor/ContentDrawer/ContentDrawerPanel.h"

#include "Render/SkeletalMesh.h"
#include "Asset/AssetManager.h"
#include "Asset/FbxImporter/FbxMeshBake.h"
#include "Text/Font.h"
#include "Editor/HitoriEd/EditorDragDrop.h"
#include "Editor/EditorUI/EditorIcons.h"
#include "Animation/AnimSequenceFile.h"
#include "Animation/BlendSpaceFile.h"
#include "Animation/AnimBlueprintFile.h"

#include "Particles/ParticleSystemFile.h"

#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace
{
	// 에셋 종류. 카드 아이콘, 아래 색 줄, 종류 글자, 필터 메뉴에 쓴다.
	enum class EAssetCategory : uint8
	{
		Folder,
		Texture,
		StaticMesh,
		SkeletalMesh,
		Animation,
		BlendSpace,
		AnimBlueprint,
		Particle,
		Level,
		Widget,
		Font,
		Sound,
		Script,
		Other,

		Count
	};

	struct FCategoryInfo
	{
		const char* Label;
		EEditorIcon Icon;
		ImU32 Color;
	};

	// EAssetCategory 순서. 색은 UE5 에셋 종류 색(카드 아래 줄)과 비슷하게 골랐다.
	const FCategoryInfo CategoryInfos[] = {
		{ "Folder",             EEditorIcon::Folder,       IM_COL32(201, 169, 110, 255) },
		{ "Texture",            EEditorIcon::Texture,      IM_COL32(214, 82, 82, 255) },
		{ "Static Mesh",        EEditorIcon::Local,        IM_COL32(63, 196, 214, 255) },
		{ "Skeletal Mesh",      EEditorIcon::SkeletalMesh, IM_COL32(236, 92, 178, 255) },
		{ "Animation Sequence", EEditorIcon::Animation,    IM_COL32(86, 170, 232, 255) },
		{ "Blend Space",        EEditorIcon::Animation,    IM_COL32(232, 176, 64, 255) },
		{ "Anim Blueprint",     EEditorIcon::Animation,    IM_COL32(230, 120, 50, 255) },
		{ "Particle System",    EEditorIcon::Particle,     IM_COL32(245, 245, 245, 255) },
		{ "Level",              EEditorIcon::Level,        IM_COL32(255, 160, 40, 255) },
		{ "Widget Blueprint",   EEditorIcon::Widget,       IM_COL32(80, 160, 255, 255) },
		{ "Font",               EEditorIcon::Font,         IM_COL32(190, 190, 190, 255) },
		{ "Sound",              EEditorIcon::Sound,        IM_COL32(130, 130, 255, 255) },
		{ "Script",             EEditorIcon::Script,       IM_COL32(242, 210, 75, 255) },
		{ "File",               EEditorIcon::File,         IM_COL32(150, 150, 150, 255) },
	};
	static_assert(IM_ARRAYSIZE(CategoryInfos) == static_cast<int>(EAssetCategory::Count));

	const FCategoryInfo& GetCategoryInfo(EAssetCategory Category)
	{
		return CategoryInfos[static_cast<int32>(Category)];
	}

	FString ToLower(FString Text)
	{
		for (char& C : Text)
			C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		return Text;
	}

	EAssetCategory GetAssetCategory(const fs::path& Path, const FString& PathString, bool bDirectory)
	{
		if (bDirectory)
			return EAssetCategory::Folder;
		// 같은 확장자를 쓰는 에셋이 있어 전용 판별 함수를 먼저 본다.
		if (AnimSequenceFile::IsAnimSequencePath(PathString)) return EAssetCategory::Animation;
		if (BlendSpaceFile::IsBlendSpacePath(PathString)) return EAssetCategory::BlendSpace;
		if (AnimBlueprintFile::IsAnimBlueprintPath(PathString)) return EAssetCategory::AnimBlueprint;
		if (ParticleSystemFile::IsParticleSystemPath(PathString)) return EAssetCategory::Particle;
		if (FFbxMeshBake::IsBakedPath(PathString))
			return PathString.ends_with("_Static.fbx.bin") ? EAssetCategory::StaticMesh : EAssetCategory::SkeletalMesh;

		const FString Ext = ToLower(Path.extension().string());
		if (Ext == ".png" || Ext == ".dds" || Ext == ".jpg" || Ext == ".jpeg" || Ext == ".tga") return EAssetCategory::Texture;
		if (Ext == ".obj") return EAssetCategory::StaticMesh;
		if (Ext == ".scene") return EAssetCategory::Level;
		if (Ext == ".widget") return EAssetCategory::Widget;
		if (Ext == ".json") return EAssetCategory::Font;   // 에셋 매니저가 .json을 폰트로 읽는다
		if (Ext == ".wav" || Ext == ".mp3" || Ext == ".ogg" || Ext == ".flac") return EAssetCategory::Sound;
		if (Ext == ".lua") return EAssetCategory::Script;
		return EAssetCategory::Other;
	}

	struct FEntry
	{
		fs::path Path;
		FString PathString;
		FString Name;
		bool bDirectory = false;
		EAssetCategory Category = EAssetCategory::Other;
	};

	bool LessIgnoreCase(const FString& A, const FString& B)
	{
		return ToLower(A) < ToLower(B);
	}

	// Text를 Width 안에서 줄바꿈해 최대 MaxLines 줄만 그린다. bCenter면 한 줄일 때 가운데 맞춘다.
	void DrawClippedText(ImDrawList* DrawList, const ImVec2& Pos, float Width, int32 MaxLines, float FontSize,
		ImU32 Color, const char* Text, bool bCenter)
	{
		ImFont* Font = ImGui::GetFont();
		const ImVec2 OneLine = Font->CalcTextSizeA(FontSize, FLT_MAX, 0.0f, Text);
		ImVec2 TextPos = Pos;
		if (bCenter && OneLine.x <= Width)
			TextPos.x += std::floor((Width - OneLine.x) * 0.5f);
		const ImVec4 Clip(Pos.x, Pos.y, Pos.x + Width, Pos.y + FontSize * MaxLines + 1.0f);
		DrawList->AddText(Font, FontSize, TextPos, Color, Text, nullptr, MaxLines > 1 ? Width : 0.0f, &Clip);
	}
}

bool FContentDrawerPanel::Init()
{
	return false;
}

void FContentDrawerPanel::Tick(float DeltaTime)
{
}

void FContentDrawerPanel::ToggleDrawer()
{
	if (bDocked)
	{
		// 레이아웃에 붙어 있으면 그 창으로 포커스만 옮긴다.
		ImGui::SetWindowFocus("Content Drawer");
		return;
	}
	bDrawerOpen = !bDrawerOpen;
	bJustOpened = bDrawerOpen;
}

bool FContentDrawerPanel::BrowseToFile(const fs::path& FilePath)
{
	std::error_code Error;
	if (FilePath.empty() || !fs::is_regular_file(FilePath, Error))
		return false;

	// 드로어는 작업 폴더 기준 상대 경로("Scenes/A.scene")로 다루므로 프로젝트 안이면 상대 경로로 바꾼다.
	fs::path Target = FilePath;
	const fs::path Relative = fs::relative(FilePath, fs::current_path(Error), Error);
	if (!Error && !Relative.empty() && *Relative.begin() != "..")
		Target = Relative;

	NavigateTo(Target.parent_path());
	HighlightPath = Target.generic_string();
	bScrollToHighlight = true;

	SetOpen(true);
	if (bDocked)
	{
		ImGui::SetWindowFocus("Content Drawer");
	}
	else
	{
		bDrawerOpen = true;
		bJustOpened = true;
	}
	return true;
}

void FContentDrawerPanel::SetDrawerRect(const ImVec2& Pos, const ImVec2& Size, ImGuiID ViewportId)
{
	DrawerPos = Pos;
	DrawerSize = Size;
	DrawerViewportId = ViewportId;
}

void FContentDrawerPanel::NavigateTo(const fs::path& Path)
{
	if (Path.empty() || Path == CurrentPath)
		return;
	BackHistory.push_back(CurrentPath);
	ForwardHistory.clear();
	CurrentPath = Path;
	SearchBuffer[0] = '\0';
	bRevealCurrentFolder = true;
}

void FContentDrawerPanel::NavigateBack()
{
	if (BackHistory.empty())
		return;
	ForwardHistory.push_back(CurrentPath);
	CurrentPath = BackHistory.back();
	BackHistory.pop_back();
	bRevealCurrentFolder = true;
}

void FContentDrawerPanel::NavigateForward()
{
	if (ForwardHistory.empty())
		return;
	BackHistory.push_back(CurrentPath);
	CurrentPath = ForwardHistory.back();
	ForwardHistory.pop_back();
	bRevealCurrentFolder = true;
}

bool FContentDrawerPanel::IsFavorite(const fs::path& Path) const
{
	return std::find(FavoritePaths.begin(), FavoritePaths.end(), Path) != FavoritePaths.end();
}

void FContentDrawerPanel::ToggleFavorite(const fs::path& Path)
{
	const auto It = std::find(FavoritePaths.begin(), FavoritePaths.end(), Path);
	if (It != FavoritePaths.end())
		FavoritePaths.erase(It);
	else
		FavoritePaths.push_back(Path);
}

void FContentDrawerPanel::OnRender()
{
	if (!BeginWindow())
		return;

	RenderContents();
	ImGui::End();
}

// 드로어 모드면 상태 표시줄 바로 위에 덮어 그리고, 포커스를 잃거나 Esc를 누르면 닫는다.
bool FContentDrawerPanel::BeginWindow()
{
	if (bDocked)
	{
		if (bPendingDock)
		{
			// 처음 레이아웃에 붙일 때는 Output Log가 있는 아래쪽 노드로 보낸다.
			if (ImGuiWindow* LogWindow = ImGui::FindWindowByName("Output Log"); LogWindow && LogWindow->DockId != 0)
				ImGui::SetNextWindowDockID(LogWindow->DockId, ImGuiCond_Always);
			bPendingDock = false;
		}
		// 도킹된 탭의 X를 누르면 패널을 없애지 않고 닫힌 드로어로 돌아간다 (상태 표시줄 버튼으로 다시 연다).
		bool bStayDocked = true;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		EditorPanelUI::BeginPanel("Content Drawer", &bStayDocked);
		ImGui::PopStyleVar();
		if (!bStayDocked)
		{
			bDocked = false;
			bDrawerOpen = false;
		}
		return true;
	}

	if (!bDrawerOpen || DrawerSize.x <= 0.0f || DrawerSize.y <= 0.0f)
		return false;

	ImGui::SetNextWindowPos(DrawerPos);
	ImGui::SetNextWindowSize(DrawerSize);
	if (DrawerViewportId != 0)
		ImGui::SetNextWindowViewport(DrawerViewportId);
	if (bJustOpened)
		ImGui::SetNextWindowFocus();

	const ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoSavedSettings;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Content Drawer###ContentDrawerOverlay", nullptr, Flags);
	ImGui::PopStyleVar(2);

	if (!bJustOpened)
	{
		const bool bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		const bool bPopupOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
		const bool bDragging = ImGui::GetDragDropPayload() != nullptr;
		// 이번 프레임은 그대로 그리고 다음 프레임부터 닫힌다.
		if ((!bFocused && !bPopupOpen && !bDragging) || (bFocused && ImGui::IsKeyPressed(ImGuiKey_Escape)))
			bDrawerOpen = false;
	}
	bJustOpened = false;
	return true;
}

// 왼쪽 사이드바 | 끌 수 있는 경계 | 오른쪽(툴바, 검색 줄, 그리드, 항목 수)
void FContentDrawerPanel::RenderContents()
{
	std::error_code Error;
	if (!fs::is_directory(CurrentPath, Error))
		CurrentPath = AssetRootPath;

	Error.clear();
	if (!fs::is_directory(AssetRootPath, Error))
	{
		ImGui::TextUnformatted("Asset folder is unavailable.");
		return;
	}

	const float S = EditorIcons::GetScale();

	DrawSidebar();

	// 사이드바 너비 조절 경계
	ImGui::SameLine(0.0f, 0.0f);
	const ImVec2 SplitterMin = ImGui::GetCursorScreenPos();
	const float SplitterWidth = 4.0f * S;
	ImGui::InvisibleButton("##SidebarSplitter", ImVec2(SplitterWidth, ImMax(1.0f, ImGui::GetContentRegionAvail().y)));
	const bool bSplitterActive = ImGui::IsItemActive();
	if (bSplitterActive)
		SidebarWidth = ImClamp(SidebarWidth + ImGui::GetIO().MouseDelta.x / S, 140.0f, 520.0f);
	if (bSplitterActive || ImGui::IsItemHovered())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	const float LineX = std::floor(SplitterMin.x + SplitterWidth * 0.5f);
	ImGui::GetWindowDrawList()->AddLine(ImVec2(LineX, SplitterMin.y), ImVec2(LineX, ImGui::GetItemRectMax().y),
		ImGui::GetColorU32(bSplitterActive ? ImGuiCol_SeparatorActive : ImGuiCol_Separator), 1.0f);

	ImGui::SameLine(0.0f, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f * S, 6.0f * S));
	ImGui::BeginChild("##ContentMain", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);
	ImGui::PopStyleVar();
	{
		DrawToolbar();
		DrawSearchRow();

		const float FooterHeight = ImGui::GetTextLineHeight() + 6.0f * S;
		ImGui::BeginChild("##AssetGrid", ImVec2(0.0f, -FooterHeight));
		DrawAssetGrid();
		BlendSpaceCreator.ProcessCreateRequest();
		AnimBlueprintCreator.ProcessCreateRequest();
		ImGui::EndChild();

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f * S);
		ImGui::TextDisabled("%d item%s (%d selected)", VisibleItemCount, VisibleItemCount == 1 ? "" : "s", bSelectionVisible ? 1 : 0);
	}
	ImGui::EndChild();

	DrawCreatePopups();
}

// ---------------- 사이드바 ----------------

// 접고 펴는 섹션 머리줄 (UE5 "Favorites", 프로젝트 이름 줄). 펼쳐져 있으면 true.
bool FContentDrawerPanel::DrawSidebarSection(const char* Label, bool& bOpen, bool* bSearchToggle)
{
	const float S = EditorIcons::GetScale();
	const float H = ImGui::GetFontSize() + 12.0f * S;
	const float Width = ImGui::GetContentRegionAvail().x;
	const ImVec2 Min = ImGui::GetCursorScreenPos();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	ImGui::PushID(Label);
	if (ImGui::InvisibleButton("##Header", ImVec2(ImMax(1.0f, Width - (bSearchToggle ? H : 0.0f)), H)))
		bOpen = !bOpen;
	const bool bHovered = ImGui::IsItemHovered();
	DrawList->AddRectFilled(Min, ImVec2(Min.x + Width, Min.y + H),
		ImGui::GetColorU32(bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_TitleBg));

	const float MidY = Min.y + H * 0.5f;
	EditorIcons::Draw(DrawList, bOpen ? EEditorIcon::ChevronDown : EEditorIcon::ChevronRight,
		ImVec2(Min.x + 12.0f * S, MidY), 8.0f * S, ImGui::GetColorU32(ImGuiCol_Text, 0.8f));
	// 굵은 글꼴이 없어 반 픽셀 옮겨 두 번 써서 굵게 보이게 한다.
	const ImVec2 TextPos(Min.x + 22.0f * S, MidY - ImGui::GetFontSize() * 0.5f);
	DrawList->AddText(TextPos, ImGui::GetColorU32(ImGuiCol_Text), Label);
	DrawList->AddText(ImVec2(TextPos.x + 0.6f * S, TextPos.y), ImGui::GetColorU32(ImGuiCol_Text), Label);

	if (bSearchToggle)
	{
		ImGui::SameLine(0.0f, 0.0f);
		if (EditorIcons::Button("##Search", EEditorIcon::Search, ImGui::GetColorU32(ImGuiCol_Text, 0.75f), nullptr, false, *bSearchToggle, H))
		{
			*bSearchToggle = !*bSearchToggle;
			if (*bSearchToggle)
				bOpen = true;
		}
		ImGui::SetItemTooltip("Search folders");
	}
	ImGui::PopID();
	return bOpen;
}

void FContentDrawerPanel::DrawSidebar()
{
	const float S = EditorIcons::GetScale();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 1.0f * S));
	ImGui::BeginChild("##ContentSidebar", ImVec2(SidebarWidth * S, 0.0f));

	const ImU32 FolderColor = GetCategoryInfo(EAssetCategory::Folder).Color;
	const float RowIndent = 14.0f * S;

	if (DrawSidebarSection("Favorites", bFavoritesOpen, nullptr))
	{
		ImGui::Dummy(ImVec2(0.0f, 2.0f * S));
		if (FavoritePaths.empty())
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + RowIndent);
			ImGui::TextDisabled("Right-click a folder to add it");
		}
		for (size_t I = 0; I < FavoritePaths.size(); ++I)
		{
			const fs::path Favorite = FavoritePaths[I];
			ImGui::PushID(static_cast<int>(I));
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + RowIndent);
			const ImVec2 RowMin = ImGui::GetCursorScreenPos();
			if (ImGui::Selectable("##Favorite", CurrentPath == Favorite, ImGuiSelectableFlags_SpanAvailWidth,
				ImVec2(0.0f, ImGui::GetFontSize() + 4.0f * S)))
				NavigateTo(Favorite);
			const float MidY = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			EditorIcons::Draw(DrawList, EEditorIcon::Folder, ImVec2(RowMin.x + 8.0f * S, MidY), 14.0f * S, FolderColor);
			DrawList->AddText(ImVec2(RowMin.x + 20.0f * S, MidY - ImGui::GetFontSize() * 0.5f),
				ImGui::GetColorU32(ImGuiCol_Text), Favorite.filename().string().c_str());
			if (ImGui::BeginPopupContextItem("##FavoriteContext"))
			{
				if (ImGui::MenuItem("Remove from Favorites"))
					ToggleFavorite(Favorite);
				ImGui::EndPopup();
			}
			ImGui::PopID();
		}
		ImGui::Dummy(ImVec2(0.0f, 4.0f * S));
	}

	if (DrawSidebarSection("Content", bContentOpen, &bShowFolderSearch))
	{
		if (bShowFolderSearch)
		{
			ImGui::Dummy(ImVec2(0.0f, 2.0f * S));
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0f * S);
			EditorIcons::SearchBox("##FolderSearch", FolderSearchBuffer, sizeof(FolderSearchBuffer), "Search Folders",
				ImGui::GetContentRegionAvail().x - 6.0f * S);
		}
		ImGui::Dummy(ImVec2(0.0f, 2.0f * S));

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * S, 2.0f * S));
		if (FolderSearchBuffer[0] != '\0' && bShowFolderSearch)
		{
			// 검색 중에는 트리 대신 이름이 맞는 폴더를 평평하게 보여 준다.
			DrawFolderSearchResults(AssetRootPath);
			DrawFolderSearchResults(SceneRootPath);
		}
		else
		{
			DrawFolderTree(AssetRootPath);
			DrawFolderTree(SceneRootPath);
			bRevealCurrentFolder = false;
		}
		ImGui::PopStyleVar();
	}

	ImGui::EndChild();
	ImGui::PopStyleVar(2);
}

// 폴더 하나를 트리 노드로 그린다 (폴더 아이콘 + 이름). Path 아래 폴더를 재귀로 그린다.
void FContentDrawerPanel::DrawFolderTree(const fs::path& Path)
{
	std::error_code Error;
	if (!fs::is_directory(Path, Error))
		return;

	const float S = EditorIcons::GetScale();
	const bool bRoot = Path == AssetRootPath || Path == SceneRootPath;

	// 하위 폴더가 없으면 화살표 없는 잎 노드로 그린다.
	bool bHasChildren = false;
	for (fs::directory_iterator It(Path, Error), End; !Error && It != End; It.increment(Error))
	{
		std::error_code TypeError;
		if (fs::is_directory(It->path(), TypeError))
		{
			bHasChildren = true;
			break;
		}
	}

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
	if (bRoot) Flags |= ImGuiTreeNodeFlags_DefaultOpen;
	if (!bHasChildren) Flags |= ImGuiTreeNodeFlags_Leaf;
	if (Path == CurrentPath) Flags |= ImGuiTreeNodeFlags_Selected;

	// BrowseToFile 등으로 들어온 폴더가 보이게 그 조상 노드를 연다.
	const FString PathString = Path.generic_string();
	const FString CurrentString = CurrentPath.generic_string();
	if (bRevealCurrentFolder && CurrentString.size() > PathString.size() && CurrentString.starts_with(PathString + "/"))
		ImGui::SetNextItemOpen(true, ImGuiCond_Always);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f * S, 2.0f * S));
	const bool bOpen = ImGui::TreeNodeEx(PathString.c_str(), Flags, "%s", "");
	ImGui::PopStyleVar();
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		NavigateTo(Path);

	// 아이콘과 이름은 직접 그린다 (트리 노드 글자 자리는 비워 둠).
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	const float MidY = (Min.y + Max.y) * 0.5f;
	const float X = Min.x + ImGui::GetTreeNodeToLabelSpacing();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	EditorIcons::Draw(DrawList, EEditorIcon::Folder, ImVec2(X + 7.0f * S, MidY), 14.0f * S, GetCategoryInfo(EAssetCategory::Folder).Color);
	DrawList->AddText(ImVec2(X + 19.0f * S, MidY - ImGui::GetFontSize() * 0.5f), ImGui::GetColorU32(ImGuiCol_Text),
		Path.filename().string().c_str());

	if (ImGui::BeginPopupContextItem("##FolderContext"))
	{
		if (ImGui::MenuItem(IsFavorite(Path) ? "Remove from Favorites" : "Add to Favorites"))
			ToggleFavorite(Path);
		ImGui::EndPopup();
	}

	if (bOpen)
	{
		for (fs::directory_iterator It(Path, Error), End; !Error && It != End; It.increment(Error))
		{
			std::error_code TypeError;
			if (fs::is_directory(It->path(), TypeError))
				DrawFolderTree(It->path());
		}
		ImGui::TreePop();
	}
}

void FContentDrawerPanel::DrawFolderSearchResults(const fs::path& Path)
{
	std::error_code Error;
	if (!fs::is_directory(Path, Error))
		return;

	const float S = EditorIcons::GetScale();
	if (EditorIcons::MatchesFilter(Path.filename().string().c_str(), FolderSearchBuffer))
	{
		const FString PathString = Path.generic_string();
		ImGui::PushID(PathString.c_str());
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f * S);
		const ImVec2 RowMin = ImGui::GetCursorScreenPos();
		if (ImGui::Selectable("##Result", CurrentPath == Path, ImGuiSelectableFlags_SpanAvailWidth,
			ImVec2(0.0f, ImGui::GetFontSize() + 4.0f * S)))
			NavigateTo(Path);
		ImGui::SetItemTooltip("%s", PathString.c_str());
		const float MidY = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		EditorIcons::Draw(DrawList, EEditorIcon::Folder, ImVec2(RowMin.x + 8.0f * S, MidY), 14.0f * S,
			GetCategoryInfo(EAssetCategory::Folder).Color);
		DrawList->AddText(ImVec2(RowMin.x + 20.0f * S, MidY - ImGui::GetFontSize() * 0.5f),
			ImGui::GetColorU32(ImGuiCol_Text), Path.filename().string().c_str());
		ImGui::PopID();
	}
	for (fs::directory_iterator It(Path, Error), End; !Error && It != End; It.increment(Error))
	{
		std::error_code TypeError;
		if (fs::is_directory(It->path(), TypeError))
			DrawFolderSearchResults(It->path());
	}
}

// ---------------- 툴바와 검색 줄 ----------------

void FContentDrawerPanel::DrawToolbar()
{
	const float S = EditorIcons::GetScale();
	const float H = 26.0f * S;
	const ImU32 IconColor = ImGui::GetColorU32(ImGuiCol_Text, 0.85f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * S, 6.0f * S));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f * S, 6.0f * S));

	// 배경이 깔린 버튼 (UE5 Add / Import / Save All)
	auto FramedButton = [&](const char* Id, EEditorIcon Icon, ImU32 Color, const char* Label, bool bDropdown)
	{
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->ChannelsSplit(2);
		DrawList->ChannelsSetCurrent(1);
		const bool bPressed = EditorIcons::Button(Id, Icon, Color, Label, bDropdown, false, H);
		DrawList->ChannelsSetCurrent(0);
		DrawList->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGuiCol_Button), 3.0f * S);
		DrawList->ChannelsMerge();
		return bPressed;
	};
	auto OpenBelow = [S](const char* PopupId)
	{
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 2.0f * S), ImGuiCond_Appearing);
		ImGui::OpenPopup(PopupId);
	};

	// Add
	if (FramedButton("##Add", EEditorIcon::Plus, EditorIcons::PlayGreen, "Add", false))
		OpenBelow("##AddMenu");
	ImGui::SetItemTooltip("Create a new asset or folder in this folder");
	if (ImGui::BeginPopup("##AddMenu"))
	{
		EditorIcons::MenuSection("Create");
		if (EditorIcons::MenuRow("New Folder", EEditorIcon::Folder))
		{
			strcpy_s(NewAssetName, "NewFolder");
			bOpenNewFolder = true;
		}
		if (EditorIcons::MenuRow("Widget Blueprint", EEditorIcon::Widget))
		{
			strcpy_s(NewAssetName, "NewWidget");
			bOpenCreateWidget = true;
		}
		if (EditorIcons::MenuRow("Particle System", EEditorIcon::Particle))
		{
			strcpy_s(NewAssetName, "NewParticleSystem");
			bOpenCreateParticle = true;
		}
		ImGui::EndPopup();
	}

	// Import
	ImGui::SameLine();
	if (FramedButton("##Import", EEditorIcon::Import, IconColor, "Import", false))
		OpenBelow("##ImportMenu");
	ImGui::SetItemTooltip("Import FBX");
	if (ImGui::BeginPopup("##ImportMenu"))
	{
		EditorIcons::MenuSection("Import FBX");
		if (EditorIcons::MenuRow("Static Mesh...", EEditorIcon::Local, false, OnImportFbx != nullptr) && OnImportFbx)
			OnImportFbx(EFbxImportMode::Static);
		if (EditorIcons::MenuRow("Skeletal Mesh...", EEditorIcon::SkeletalMesh, false, OnImportFbx != nullptr) && OnImportFbx)
			OnImportFbx(EFbxImportMode::Skeletal);
		ImGui::EndPopup();
	}

	// Save All
	ImGui::SameLine();
	ImGui::BeginDisabled(!OnSaveAll);
	if (FramedButton("##SaveAll", EEditorIcon::Save, IconColor, "Save All", false) && OnSaveAll)
		OnSaveAll();
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("Save the current level");

	// 뒤로 / 앞으로
	ImGui::SameLine(0.0f, 10.0f * S);
	ImGui::BeginDisabled(BackHistory.empty());
	if (EditorIcons::Button("##Back", EEditorIcon::ArrowLeft, IconColor, nullptr, false, false, H))
		NavigateBack();
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("Back");
	ImGui::SameLine(0.0f, 0.0f);
	ImGui::BeginDisabled(ForwardHistory.empty());
	if (EditorIcons::Button("##Forward", EEditorIcon::ArrowRight, IconColor, nullptr, false, false, H))
		NavigateForward();
	ImGui::EndDisabled();
	ImGui::SetItemTooltip("Forward");

	// 오른쪽 버튼 자리를 먼저 계산한다.
	const char* DockLabel = bDocked ? "Undock to Drawer" : "Dock in Layout";
	const float DockWidth = EditorIcons::CalcButtonWidth(true, DockLabel, false, H);
	const float GearWidth = EditorIcons::CalcButtonWidth(true, nullptr, false, H);
	const float RightWidth = DockWidth + GearWidth + 4.0f * S;

	// 경로 (All > Assets > Particle). 조각을 누르면 그 폴더로 간다.
	ImGui::SameLine(0.0f, 8.0f * S);
	const ImVec2 CrumbMin = ImGui::GetCursorScreenPos();
	const float CrumbWidth = ImMax(60.0f * S, ImGui::GetContentRegionAvail().x - RightWidth - 8.0f * S);
	const ImVec2 CrumbMax(CrumbMin.x + CrumbWidth, CrumbMin.y + H);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(CrumbMin, CrumbMax, ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f * S);
	DrawList->PushClipRect(CrumbMin, CrumbMax, true);
	ImGui::SetCursorScreenPos(ImVec2(CrumbMin.x + 4.0f * S, CrumbMin.y));
	{
		std::vector<fs::path> Crumbs;
		fs::path Accumulated;
		for (const fs::path& Part : CurrentPath)
		{
			Accumulated /= Part;
			Crumbs.push_back(Accumulated);
		}

		const ImU32 ChevronColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
		auto Chevron = [&]()
		{
			ImGui::SameLine(0.0f, 0.0f);
			const ImVec2 Pos = ImGui::GetCursorScreenPos();
			ImGui::Dummy(ImVec2(12.0f * S, H));
			EditorIcons::Draw(DrawList, EEditorIcon::ChevronRight, ImVec2(Pos.x + 6.0f * S, Pos.y + H * 0.5f), 8.0f * S, ChevronColor);
			ImGui::SameLine(0.0f, 0.0f);
		};

		if (EditorIcons::Button("##CrumbAll", EEditorIcon::None, 0, "All", false, false, H))
			NavigateTo(AssetRootPath);
		for (size_t I = 0; I < Crumbs.size(); ++I)
		{
			Chevron();
			ImGui::PushID(static_cast<int>(I));
			const FString Label = Crumbs[I].filename().string();
			if (EditorIcons::Button("##Crumb", EEditorIcon::None, 0, Label.c_str(), false, false, H))
				NavigateTo(Crumbs[I]);
			ImGui::PopID();
		}
	}
	DrawList->PopClipRect();

	// Dock in Layout / 보기 설정
	ImGui::SetCursorScreenPos(ImVec2(CrumbMax.x + 8.0f * S, CrumbMin.y));
	if (EditorIcons::Button("##Dock", EEditorIcon::Dock, IconColor, DockLabel, false, false, H))
	{
		bDocked = !bDocked;
		bPendingDock = bDocked;
		bDrawerOpen = !bDocked;
		bJustOpened = bDrawerOpen;
	}
	ImGui::SameLine();
	if (EditorIcons::Button("##ViewOptions", EEditorIcon::Settings, IconColor, nullptr, false,
		ImGui::IsPopupOpen("##ViewOptionsMenu"), H))
	{
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y + 2.0f * S), ImGuiCond_Appearing, ImVec2(1.0f, 0.0f));
		ImGui::OpenPopup("##ViewOptionsMenu");
	}
	ImGui::SetItemTooltip("View Options");
	if (ImGui::BeginPopup("##ViewOptionsMenu"))
	{
		EditorIcons::MenuSection("Thumbnails");
		ImGui::SetNextItemWidth(200.0f * S);
		ImGui::SliderFloat("##ThumbnailSize", &ThumbnailSize, 56.0f, 192.0f, "%.0f px");
		ImGui::Spacing();
		EditorIcons::MenuSection("Content");
		if (EditorIcons::MenuRow("Show Folders", EEditorIcon::Folder, bShowFolders, true, true))
			bShowFolders = !bShowFolders;
		ImGui::EndPopup();
	}

	ImGui::PopStyleVar(2);
}

void FContentDrawerPanel::DrawSearchRow()
{
	const float S = EditorIcons::GetScale();
	const float H = 24.0f * S;
	const ImU32 IconColor = ImGui::GetColorU32(ImGuiCol_Text, 0.85f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f * S, 6.0f * S));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f * S, 6.0f * S));

	// 필터 (종류별로 켜고 끈다)
	const bool bFiltered = TypeFilterMask != 0;
	if (EditorIcons::Button("##Filters", EEditorIcon::Filter, bFiltered ? ImGui::GetColorU32(ImGuiCol_SliderGrab) : IconColor,
		nullptr, true, ImGui::IsPopupOpen("##FilterMenu"), H))
	{
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 2.0f * S), ImGuiCond_Appearing);
		ImGui::OpenPopup("##FilterMenu");
	}
	ImGui::SetItemTooltip("Filters");
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * S, 1.0f * S));
	if (ImGui::BeginPopup("##FilterMenu"))
	{
		EditorIcons::MenuSection("Asset Types");
		for (int32 I = 1; I < static_cast<int32>(EAssetCategory::Count); ++I)
		{
			const FCategoryInfo& Info = CategoryInfos[I];
			const uint32 Bit = 1u << I;
			if (EditorIcons::MenuRow(Info.Label, Info.Icon, (TypeFilterMask & Bit) != 0, true, true))
				TypeFilterMask ^= Bit;
		}
		ImGui::Separator();
		if (EditorIcons::MenuRow("Reset Filters", EEditorIcon::Reset, false, bFiltered))
			TypeFilterMask = 0;
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();

	// 검색 (지금 폴더 안 이름)
	ImGui::SameLine();
	char Hint[160];
	std::snprintf(Hint, sizeof(Hint), "Search %s", CurrentPath.filename().string().c_str());
	const float SortWidth = EditorIcons::CalcButtonWidth(true, nullptr, true, H);
	const float SearchWidth = ImMin(ImGui::GetContentRegionAvail().x - SortWidth - 8.0f * S, 520.0f * S);
	EditorIcons::SearchBox("##AssetSearch", SearchBuffer, sizeof(SearchBuffer), Hint, ImMax(80.0f * S, SearchWidth));

	// 정렬
	ImGui::SameLine();
	if (EditorIcons::Button("##Sort", EEditorIcon::Sort, IconColor, nullptr, true, ImGui::IsPopupOpen("##SortMenu"), H))
	{
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y + 2.0f * S), ImGuiCond_Appearing);
		ImGui::OpenPopup("##SortMenu");
	}
	ImGui::SetItemTooltip("Sort");
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * S, 1.0f * S));
	if (ImGui::BeginPopup("##SortMenu"))
	{
		EditorIcons::MenuSection("Sort By");
		if (EditorIcons::MenuRow("Name (A to Z)", EEditorIcon::None, SortMode == ESortMode::NameAscending, true, false, nullptr, true))
			SortMode = ESortMode::NameAscending;
		if (EditorIcons::MenuRow("Name (Z to A)", EEditorIcon::None, SortMode == ESortMode::NameDescending, true, false, nullptr, true))
			SortMode = ESortMode::NameDescending;
		if (EditorIcons::MenuRow("Asset Type", EEditorIcon::None, SortMode == ESortMode::Type, true, false, nullptr, true))
			SortMode = ESortMode::Type;
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();

	ImGui::PopStyleVar(2);
}

// ---------------- 에셋 그리드 ----------------

void FContentDrawerPanel::DrawAssetGrid()
{
	const float S = EditorIcons::GetScale();

	// 빈 곳 우클릭: 만들기 메뉴
	if (ImGui::BeginPopupContextWindow("##DrawerContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("New Folder")) { strcpy_s(NewAssetName, "NewFolder"); bOpenNewFolder = true; }
		if (ImGui::MenuItem("Create Widget")) { strcpy_s(NewAssetName, "NewWidget"); bOpenCreateWidget = true; }
		if (ImGui::MenuItem("Create Particle System")) { strcpy_s(NewAssetName, "NewParticleSystem"); bOpenCreateParticle = true; }
		ImGui::EndPopup();
	}

	// 마우스 뒤로/앞으로 버튼
	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
	{
		if (ImGui::IsMouseClicked(3)) NavigateBack();
		if (ImGui::IsMouseClicked(4)) NavigateForward();
	}

	// 지금 폴더 항목을 모아 거르고 정렬한다.
	std::vector<FEntry> Entries;
	{
		std::error_code IterateError;
		for (fs::directory_iterator It(CurrentPath, IterateError), End; !IterateError && It != End; It.increment(IterateError))
		{
			std::error_code TypeError;
			FEntry Entry;
			Entry.Path = It->path();
			Entry.bDirectory = fs::is_directory(Entry.Path, TypeError);
			if (TypeError || (Entry.bDirectory && !bShowFolders))
				continue;
			Entry.PathString = Entry.Path.generic_string();
			Entry.Name = Entry.Path.filename().string();
			if (!EditorIcons::MatchesFilter(Entry.Name.c_str(), SearchBuffer))
				continue;
			Entry.Category = GetAssetCategory(Entry.Path, Entry.PathString, Entry.bDirectory);
			if (TypeFilterMask != 0 && !Entry.bDirectory && (TypeFilterMask & (1u << static_cast<uint32>(Entry.Category))) == 0)
				continue;
			Entries.push_back(std::move(Entry));
		}
	}
	std::sort(Entries.begin(), Entries.end(), [this](const FEntry& A, const FEntry& B)
	{
		if (A.bDirectory != B.bDirectory)
			return A.bDirectory;   // 폴더 먼저
		if (SortMode == ESortMode::Type && A.Category != B.Category)
			return A.Category < B.Category;
		return SortMode == ESortMode::NameDescending ? LessIgnoreCase(B.Name, A.Name) : LessIgnoreCase(A.Name, B.Name);
	});
	VisibleItemCount = static_cast<int32>(Entries.size());
	bSelectionVisible = false;

	if (Entries.empty())
	{
		ImGui::Dummy(ImVec2(0.0f, 12.0f * S));
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8.0f * S);
		ImGui::TextDisabled(SearchBuffer[0] || TypeFilterMask ? "No items match the current search or filters." : "This folder is empty.");
		return;
	}

	// 카드 크기와 열 수
	const float Thumb = std::floor(ThumbnailSize * S);
	const float FontSize = ImGui::GetFontSize();
	const float TypeFontSize = std::floor(FontSize * 0.85f);
	const float CardWidth = Thumb;
	const float CardHeight = Thumb + 6.0f * S + FontSize * 2.0f + 4.0f * S + TypeFontSize + 6.0f * S;
	const float Gap = 10.0f * S;
	const float Avail = ImGui::GetContentRegionAvail().x - 8.0f * S;
	const int32 Columns = ImMax(1, static_cast<int32>((Avail + Gap) / (CardWidth + Gap)));

	ImGui::Dummy(ImVec2(0.0f, 4.0f * S));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Gap, Gap));
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const float Rounding = 4.0f * S;

	// 폴더를 열 때 멈추지 않도록 썸네일은 프레임당 하나씩 로드한다. 그동안은 종류 아이콘을 보여 준다.
	bool bLoadedThumbnail = false;

	for (size_t Index = 0; Index < Entries.size(); ++Index)
	{
		const FEntry& Entry = Entries[Index];
		const FCategoryInfo& Info = GetCategoryInfo(Entry.Category);
		if (Index % Columns == 0)
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f * S);
		else
			ImGui::SameLine();

		ImGui::PushID(Entry.PathString.c_str());

		// 실제 에셋일 때만 채워진다. 끌어다 놓기 원본이 된다.
		UTexture2D* TextureAsset = nullptr;
		UFont* FontAsset = nullptr;
		UTexture2D* Thumbnail = nullptr;
		if (!Entry.bDirectory)
		{
			URenderAsset* Asset = UAssetManager::FindLoadedAsset(Entry.PathString);
			if (!Asset && !bLoadedThumbnail)
			{
				// 메시는 썸네일로 쓰지 않으므로 드로어를 연 것만으로 로드하지 않는다
				const EAssetType UnloadedType = UAssetManager::GetUnloadedAssetType(Entry.PathString);
				if (UnloadedType == EAssetType::Texture || UnloadedType == EAssetType::Font)
				{
					Asset = UAssetManager::GetAssetByPath<URenderAsset>(Entry.PathString);
					bLoadedThumbnail = true;
				}
			}
			if (Asset && Asset->IsA<UTexture2D>())
			{
				TextureAsset = Cast<UTexture2D>(Asset);
				Thumbnail = TextureAsset;
			}
			else if (Asset && Asset->IsA<UFont>())
			{
				FontAsset = Cast<UFont>(Asset);
				Thumbnail = FontAsset->AtlasTexture;   // 폰트는 글리프 아틀라스를 미리보기로 쓴다
			}
		}

		const ImVec2 Min = ImGui::GetCursorScreenPos();
		const ImVec2 Max(Min.x + CardWidth, Min.y + CardHeight);
		ImGui::InvisibleButton("##Item", ImVec2(CardWidth, CardHeight));
		const bool bHovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
			HighlightPath = Entry.PathString;
		const bool bSelected = Entry.PathString == HighlightPath;
		bSelectionVisible |= bSelected;
		if (bSelected && bScrollToHighlight)
		{
			ImGui::SetScrollHereY(0.5f);
			bScrollToHighlight = false;
		}

		const ImU32 TextColor = ImGui::GetColorU32(ImGuiCol_Text);
		if (Entry.bDirectory)
		{
			// 폴더: 카드 없이 큰 폴더 아이콘 + 이름 (UE5와 같다)
			if (bSelected || bHovered)
				DrawList->AddRectFilled(Min, Max, ImGui::GetColorU32(bSelected ? ImGuiCol_Header : ImGuiCol_ButtonHovered), Rounding);
			EditorIcons::Draw(DrawList, EEditorIcon::Folder, ImVec2(Min.x + CardWidth * 0.5f, Min.y + Thumb * 0.5f + 2.0f * S),
				Thumb * 0.8f, Info.Color);
			DrawClippedText(DrawList, ImVec2(Min.x + 4.0f * S, Min.y + Thumb + 6.0f * S), CardWidth - 8.0f * S, 2,
				FontSize, TextColor, Entry.Name.c_str(), true);
		}
		else
		{
			// 파일: 위쪽 썸네일 + 종류 색 줄 + 아래쪽 이름 / 종류
			const float InfoTop = Min.y + Thumb;
			DrawList->AddRectFilled(Min, ImVec2(Max.x, InfoTop), ImGui::GetColorU32(ImGuiCol_FrameBg), Rounding, ImDrawFlags_RoundCornersTop);
			DrawList->AddRectFilled(ImVec2(Min.x, InfoTop), Max,
				ImGui::GetColorU32(bSelected ? ImGuiCol_ButtonActive : bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button),
				Rounding, ImDrawFlags_RoundCornersBottom);
			DrawList->AddRectFilled(ImVec2(Min.x, InfoTop - 2.0f * S), ImVec2(Max.x, InfoTop), Info.Color);

			if (Thumbnail && Thumbnail->GetResource() && Thumbnail->GetResource()->GetSRV())
			{
				// 가로세로 비율을 지켜 썸네일 칸에 맞춘다.
				const float Pad = 4.0f * S;
				const float Box = Thumb - Pad * 2.0f - 2.0f * S;
				float W = Box, Hgt = Box;
				const float TexW = static_cast<float>(Thumbnail->GetWidth());
				const float TexH = static_cast<float>(Thumbnail->GetHeight());
				if (TexW > 0.0f && TexH > 0.0f)
				{
					if (TexW >= TexH) Hgt = Box * TexH / TexW;
					else W = Box * TexW / TexH;
				}
				const ImVec2 ImgMin(std::floor(Min.x + (CardWidth - W) * 0.5f), std::floor(Min.y + Pad + (Box - Hgt) * 0.5f));
				DrawList->AddImage((ImTextureID)Thumbnail->GetResource()->GetSRV(), ImgMin, ImVec2(ImgMin.x + W, ImgMin.y + Hgt));
			}
			else
			{
				EditorIcons::Draw(DrawList, Info.Icon, ImVec2(Min.x + CardWidth * 0.5f, Min.y + Thumb * 0.5f - 1.0f * S),
					std::floor(Thumb * 0.42f), Info.Color);
			}

			DrawClippedText(DrawList, ImVec2(Min.x + 4.0f * S, InfoTop + 6.0f * S), CardWidth - 8.0f * S, 2,
				FontSize, TextColor, Entry.Name.c_str(), false);
			DrawClippedText(DrawList, ImVec2(Min.x + 4.0f * S, Max.y - TypeFontSize - 5.0f * S), CardWidth - 8.0f * S, 1,
				TypeFontSize, ImGui::GetColorU32(bSelected ? ImGuiCol_Text : ImGuiCol_TextDisabled, bSelected ? 0.8f : 1.0f),
				Entry.Category == EAssetCategory::Other ? Entry.Path.extension().string().c_str() : Info.Label, false);
		}
		if (bSelected)
			DrawList->AddRect(Min, Max, ImGui::GetColorU32(ImGuiCol_SeparatorActive), Rounding, 0, ImMax(1.0f, 1.5f * S));
		else if (bHovered)
			DrawList->AddRect(Min, Max, ImGui::GetColorU32(ImGuiCol_Text, 0.25f), Rounding, 0, 1.0f);

		if (bHovered && !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			ImGui::SetTooltip("%s\n%s", Entry.Name.c_str(), Entry.Category == EAssetCategory::Other ? "File" : Info.Label);

		// 우클릭 메뉴: 폴더는 즐겨찾기, 스켈레탈 메시는 블렌드 스페이스 / 애님 블루프린트 만들기
		const bool bSkeletalMesh = Entry.Category == EAssetCategory::SkeletalMesh;
		if ((Entry.bDirectory || bSkeletalMesh) && ImGui::BeginPopupContextItem("##AssetContext"))
		{
			if (Entry.bDirectory)
			{
				if (ImGui::MenuItem(IsFavorite(Entry.Path) ? "Remove from Favorites" : "Add to Favorites"))
					ToggleFavorite(Entry.Path);
			}
			else
			{
				BlendSpaceCreator.DrawMenuItems(Entry.PathString, CurrentPath);
				AnimBlueprintCreator.DrawMenuItems(Entry.PathString, CurrentPath);
			}
			ImGui::EndPopup();
		}

		// 페이로드에 포인터를 그대로 실어서 받는 쪽이 AssetMap을 다시 뒤지지 않게 한다.
		if (TextureAsset && ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload(EditorDragDrop::Texture, &TextureAsset, sizeof(UTexture2D*));
			ImGui::Image((ImTextureID)TextureAsset->GetResource()->GetSRV(), { 64.0f * S, 64.0f * S });
			ImGui::TextUnformatted(Entry.Name.c_str());
			ImGui::EndDragDropSource();
		}
		if (FontAsset && ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload(EditorDragDrop::Font, &FontAsset, sizeof(UFont*));
			ImGui::TextUnformatted(Entry.Name.c_str());
			ImGui::EndDragDropSource();
		}

		// 더블 클릭: 폴더는 들어가고, 에셋은 맞는 에디터로 연다.
		if (bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (Entry.bDirectory)
			{
				NavigateTo(Entry.Path);
			}
			// TODO: UIEditor를 FAssetEditor 계열로 통합
			else if (Entry.Category == EAssetCategory::Widget && OnOpenWidget)
			{
				OnOpenWidget(Entry.PathString);
			}
			else if ((Entry.Path.extension() == ".obj" || FFbxMeshBake::IsBakedPath(Entry.PathString)
				|| Entry.Category == EAssetCategory::Animation || Entry.Category == EAssetCategory::BlendSpace
				|| Entry.Category == EAssetCategory::AnimBlueprint || Entry.Category == EAssetCategory::Particle) && OnOpenMesh)
			{
				OnOpenMesh(Entry.PathString);
			}
		}

		ImGui::PopID();
	}
	ImGui::PopStyleVar();
	ImGui::Dummy(ImVec2(0.0f, 4.0f * S));
}

// 만들기 창. 메뉴 안에서 바로 열 수 없어 플래그만 세우고 여기(창 기준)에서 연다.
void FContentDrawerPanel::DrawCreatePopups()
{
	if (bOpenCreateWidget) { ImGui::OpenPopup("Create Widget"); bOpenCreateWidget = false; }
	if (bOpenCreateParticle) { ImGui::OpenPopup("Create Particle System"); bOpenCreateParticle = false; }
	if (bOpenNewFolder) { ImGui::OpenPopup("New Folder"); bOpenNewFolder = false; }

	// 이름 입력 + 같은 이름 검사 + Create/Cancel. Create를 누르면 OnCreate(만들 경로)를 부른다.
	auto NameDialog = [this](const char* Title, const char* Extension, const std::function<void(const fs::path&)>& OnCreate)
	{
		if (!ImGui::BeginPopupModal(Title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			return;
		if (ImGui::IsWindowAppearing())
			ImGui::SetKeyboardFocusHere();
		const bool bEnter = ImGui::InputText("Name", NewAssetName, sizeof(NewAssetName), ImGuiInputTextFlags_EnterReturnsTrue);
		const fs::path Target = CurrentPath / (FString(NewAssetName) + Extension);
		std::error_code Error;
		const bool bExists = fs::exists(Target, Error);
		if (bExists) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "A file with this name already exists.");

		const bool bValid = !bExists && NewAssetName[0] != '\0';
		ImGui::BeginDisabled(!bValid);
		if ((ImGui::Button("Create") || (bEnter && bValid)))
		{
			OnCreate(Target);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	};

	NameDialog("Create Widget", ".widget", [this](const fs::path& Target)
	{
		if (OnCreateWidget)
			OnCreateWidget(Target.generic_string());   // "Assets/UI/GameOver.widget" 형식
	});

	NameDialog("Create Particle System", ".particle", [this](const fs::path& Target)
	{
		FString Error;
		if (ParticleSystemFile::CreateDefault(Target, Error))
		{
			HighlightPath = Target.generic_string();
			if (OnOpenMesh) OnOpenMesh(Target.generic_string());   // 만들자마자 에디터로 연다
		}
		else
		{
			HTR_LOG(Warning, "Failed to create particle system: {}", Error);
		}
	});

	NameDialog("New Folder", "", [this](const fs::path& Target)
	{
		std::error_code Error;
		if (fs::create_directory(Target, Error))
			HighlightPath = Target.generic_string();
		else
			HTR_LOG(Warning, "Failed to create folder: {}", Target.generic_string());
	});
}
