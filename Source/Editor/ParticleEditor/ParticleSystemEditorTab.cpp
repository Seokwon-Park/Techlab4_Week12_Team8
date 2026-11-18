#include "EnginePCH.h"
#include "ParticleSystemEditorTab.h"
#include "ParticleSystemEditor.h"
#include "Panels/ParticleEditorPanels.h"

FParticleSystemEditorTab::FParticleSystemEditorTab(FParticleSystemEditor& InEditor)
	: FDockedAssetEditorTab(InEditor, "ParticleDockSpace", "ParticlePanelClass")
	, ParticleEditor(InEditor)
{
	RegisterPanel(MakeUnique<FParticleViewportPanel>(InEditor));   // Panels[0]
	RegisterPanel(MakeUnique<FParticleEmittersPanel>(InEditor));   // Panels[1]
	RegisterPanel(MakeUnique<FParticleDetailsPanel>(InEditor));    // Panels[2]
	RegisterPanel(MakeUnique<FParticleCurveEditorPanel>(InEditor));  // Panels[3]
}

void FParticleSystemEditorTab::BeforeRender()
{
	// 뷰포트 패널이 안 그려진 프레임엔 크기 0 → 렌더도 카메라 입력도 쉰다
	ParticleEditor.SetViewportState(0, 0, {});
}

// Cascade 배치: 왼쪽 위 Viewport, 왼쪽 아래 Details, 오른쪽 전체 Emitters
// (나중에 Curve Editor를 붙이면 EmittersId를 아래로 한 번 더 나눈다)
void FParticleSystemEditorTab::BuildDefaultLayout(ImGuiID InDockSpaceId, const ImVec2& Size)
{
	ImGui::DockBuilderAddNode(InDockSpaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(InDockSpaceId, Size);

	ImGuiID LeftId = InDockSpaceId;
	ImGuiID EmittersId = ImGui::DockBuilderSplitNode(LeftId, ImGuiDir_Right, 0.52f, nullptr, &LeftId);
	const ImGuiID CurveId = ImGui::DockBuilderSplitNode(EmittersId, ImGuiDir_Down, 0.40f, nullptr, &EmittersId);
	ImGuiID ViewportId = LeftId;
	const ImGuiID DetailsId = ImGui::DockBuilderSplitNode(ViewportId, ImGuiDir_Down, 0.45f, nullptr, &ViewportId);


	ImGui::DockBuilderDockWindow(Panels[0]->GetWindowName(), ViewportId);
	ImGui::DockBuilderDockWindow(Panels[1]->GetWindowName(), EmittersId);
	ImGui::DockBuilderDockWindow(Panels[2]->GetWindowName(), DetailsId);
	ImGui::DockBuilderDockWindow(Panels[3]->GetWindowName(), CurveId);
	ImGui::DockBuilderFinish(InDockSpaceId);
}

void FParticleSystemEditorTab::OnRender(bool bActive)
{
	// 기반 OnRender는 GetContentRegionAvail()로 독스페이스 크기를 잡는다.
	// 그 전에 툴바를 그려 두면 독스페이스가 툴바 아래 남은 영역을 채운다.
	if (bActive && !ParticleEditor.IsCloseRequested())
	{
		DrawToolbar();
	}
	FDockedAssetEditorTab::OnRender(bActive);
}

namespace
{
	// 켜져 있으면 버튼 색을 강조색으로 (Cascade 툴바의 토글 버튼)
	bool ToggleButton(const char* Label, bool& bValue)
	{
		if (bValue) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		const bool bClicked = ImGui::Button(Label);
		if (bValue) ImGui::PopStyleColor();
		if (bClicked) bValue = !bValue;
		return bClicked;
	}

	void ToolbarSeparator()
	{
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
	}
}

void FParticleSystemEditorTab::DrawToolbar()
{
	ImGui::BeginDisabled(!ParticleEditor.IsDirty());
	if (ImGui::Button("Save")) ParticleEditor.Save();
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Restart Sim")) ParticleEditor.RestartPreview();

	ToolbarSeparator();
	ToggleButton("Bounds", ParticleEditor.bShowBounds);
	ImGui::SameLine();
	ToggleButton("Origin Axis", ParticleEditor.bShowOriginAxis);

	ToolbarSeparator();
	bool bTest = ParticleEditor.IsShowingTestData();      // 이 엔진 전용: 1번 코어 전 렌더 확인
	if (ToggleButton("Test Data", bTest)) ParticleEditor.SetShowTestData(bTest);

	ImGui::Separator();
}