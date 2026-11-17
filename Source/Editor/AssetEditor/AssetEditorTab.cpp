#include "EnginePCH.h"
#include "Editor/AssetEditor/AssetEditorTab.h"
#include "Editor/AssetEditor/AssetEditor.h"
#include "Editor/EditorUI/EditorIcons.h"
#include <filesystem>

FAssetEditorTab::FAssetEditorTab(FAssetEditor& InEditor) : Editor(InEditor)
{
	// ### 뒤 ID는 처음 연 경로로 고정한다. 에디터 안에서 애셋이 바뀌어도(애니메이션 뷰어의 Asset Browser) 같은 창으로 남는다.
	TabId = FString("###AssetEditor:") + Editor.GetAssetPath();
}

const char* FAssetEditorTab::GetTabName() const
{
	// 보이는 이름은 지금 경로로 만든다. 경로가 바뀐 때만 다시 만든다.
	if (NamedPath != Editor.GetAssetPath())
	{
		NamedPath = Editor.GetAssetPath();
		const FString Filename = std::filesystem::path(NamedPath).filename().string();
		const FString Suffix = FString(" (") + Editor.GetEditorTypeName() + ")" + TabId;
		TabName = Filename + Suffix;
		DirtyTabName = Filename + "*" + Suffix;
	}
	return Editor.IsDirty() ? DirtyTabName.c_str() : TabName.c_str();
}

bool FAssetEditorTab::GetTabIcon(EEditorIcon& OutIcon, ImU32& OutColor) const
{
	// 콘텐츠 드로어의 에셋 종류 아이콘·색과 맞춘다
	struct FTypeIcon { const char* TypeName; EEditorIcon Icon; ImU32 Color; };
	static const FTypeIcon TypeIcons[] = {
		{ "StaticMesh",     EEditorIcon::Local,        IM_COL32(63, 196, 214, 255) },
		{ "SkeletalMesh",   EEditorIcon::SkeletalMesh, IM_COL32(236, 92, 178, 255) },
		{ "AnimSequence",   EEditorIcon::Animation,    IM_COL32(86, 170, 232, 255) },
		{ "BlendSpace",     EEditorIcon::Animation,    IM_COL32(232, 176, 64, 255) },
		{ "AnimBlueprint",  EEditorIcon::Animation,    IM_COL32(230, 120, 50, 255) },
		{ "ParticleSystem", EEditorIcon::Particle,     IM_COL32(245, 245, 245, 255) },
		{ "Widget",         EEditorIcon::Widget,       IM_COL32(80, 160, 255, 255) },
	};

	const FString TypeName = Editor.GetEditorTypeName();
	for (const FTypeIcon& Entry : TypeIcons)
	{
		if (TypeName == Entry.TypeName)
		{
			OutIcon = Entry.Icon;
			OutColor = Entry.Color;
			return true;
		}
	}
	OutIcon = EEditorIcon::File;
	OutColor = IM_COL32(150, 150, 150, 255);
	return true;
}

bool FAssetEditorTab::ConfirmClose()
{
	return Editor.ConfirmClose();
}

void FAssetEditorTab::OnCloseRequested()
{
	Editor.Close();
}

void FAssetEditorTab::DrawMenuBar()
{
	// 기본 Editor Menu Bar.

	if (ImGui::BeginMenu("Editor"))
	{
		if (ImGui::MenuItem("Close Editor"))
		{
			Editor.Close();
		}

		ImGui::EndMenu();
	}
}

void FAssetEditorTab::OnRender(bool bActive)
{
}
