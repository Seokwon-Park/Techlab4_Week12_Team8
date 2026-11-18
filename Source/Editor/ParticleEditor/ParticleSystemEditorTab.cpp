#include "EnginePCH.h"
#include "ParticleSystemEditorTab.h"
#include "ParticleSystemEditor.h"
#include "Panels/ParticleEditorPanels.h"
#include "Editor/EditorUI/EditorIcons.h"

#include <functional>

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
	float UIScale() { return ImGui::GetFontSize() / 13.0f; }

	ImU32 WithStyleAlpha(ImU32 Color)
	{
		const float A = ImGui::GetStyle().Alpha;
		if (A >= 1.0f) return Color;
		const uint32 Alpha = static_cast<uint32>(((Color >> IM_COL32_A_SHIFT) & 0xFF) * A);
		return (Color & ~IM_COL32_A_MASK) | (Alpha << IM_COL32_A_SHIFT);
	}

	// 아이콘 그리기: Center를 중심으로 Size 정사각형 안
	using FIconDraw = std::function<void(ImDrawList*, ImVec2 Center, float Size)>;

	FIconDraw EngineIcon(EEditorIcon Icon, ImU32 Color = IM_COL32(220, 222, 228, 255))
	{
		return [Icon, Color](ImDrawList* DL, ImVec2 C, float Size) { EditorIcons::Draw(DL, Icon, C, Size, WithStyleAlpha(Color)); };
	}

	// Bounds: 구 + 바깥 상자 (UE Bounds 아이콘처럼)
	void DrawBoundsIcon(ImDrawList* DL, ImVec2 C, float Size)
	{
		const float R = Size * 0.5f;
		const ImU32 Col = WithStyleAlpha(IM_COL32(220, 222, 228, 255));
		const ImU32 Accent = WithStyleAlpha(IM_COL32(240, 200, 60, 255));
		DL->AddRect(ImVec2(C.x - R, C.y - R), ImVec2(C.x + R, C.y + R), Accent, 0.0f, 0, 1.4f);
		DL->AddCircle(C, R * 0.62f, Col, 0, 1.4f);
		DL->AddEllipse(C, ImVec2(R * 0.62f, R * 0.24f), Col, 0.0f, 0, 1.0f);
	}

	// Origin Axis: X 빨강, Y 초록, Z 파랑
	void DrawAxisIcon(ImDrawList* DL, ImVec2 C, float Size)
	{
		const float R = Size * 0.5f;
		const ImVec2 O(C.x - R * 0.45f, C.y + R * 0.45f);
		DL->AddLine(O, ImVec2(O.x + R * 1.35f, O.y), WithStyleAlpha(IM_COL32(230, 70, 60, 255)), 1.8f);
		DL->AddLine(O, ImVec2(O.x + R * 0.75f, O.y - R * 0.75f), WithStyleAlpha(IM_COL32(90, 200, 80, 255)), 1.8f);
		DL->AddLine(O, ImVec2(O.x, O.y - R * 1.35f), WithStyleAlpha(IM_COL32(80, 130, 240, 255)), 1.8f);
		DL->AddCircleFilled(O, 2.0f, WithStyleAlpha(IM_COL32(220, 222, 228, 255)));
	}

	// Test Data: 크기가 다른 점 몇 개 (파티클)
	void DrawTestDataIcon(ImDrawList* DL, ImVec2 C, float Size)
	{
		const float R = Size * 0.5f;
		const ImU32 Col = WithStyleAlpha(IM_COL32(240, 160, 60, 255));
		DL->AddCircleFilled(ImVec2(C.x - R * 0.45f, C.y + R * 0.35f), R * 0.32f, Col);
		DL->AddCircleFilled(ImVec2(C.x + R * 0.45f, C.y + R * 0.15f), R * 0.22f, Col);
		DL->AddCircleFilled(ImVec2(C.x + R * 0.05f, C.y - R * 0.5f), R * 0.16f, Col);
		DL->AddCircleFilled(ImVec2(C.x + R * 0.65f, C.y - R * 0.6f), R * 0.1f, Col);
	}

	// 아이콘 + 글자 버튼. 평소엔 배경 없음, hover · 누름 · 켜짐(bSelected)일 때만 칠한다 (EditorIcons::Button과 같은 모양)
	bool ToolbarButton(const char* Label, const FIconDraw& DrawIcon, bool bSelected = false, const char* Tooltip = nullptr)
	{
		const float S = UIScale();
		const float H = 28.0f * S;
		const float IconSize = 16.0f * S;
		const float PadX = 7.0f * S;
		const ImVec2 TextSize = ImGui::CalcTextSize(Label);
		const float W = PadX + IconSize + 6.0f * S + TextSize.x + PadX;

		const ImVec2 Min = ImGui::GetCursorScreenPos();
		const bool bPressed = ImGui::InvisibleButton(Label, ImVec2(W, H));
		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = ImGui::IsItemActive();

		ImDrawList* DL = ImGui::GetWindowDrawList();
		if (bHeld || bHovered || bSelected)
		{
			const ImU32 Bg = ImGui::GetColorU32(bHeld ? ImGuiCol_ButtonActive : bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Header);
			DL->AddRectFilled(Min, ImVec2(Min.x + W, Min.y + H), Bg, 3.0f * S);
		}
		const float MidY = Min.y + H * 0.5f;
		DrawIcon(DL, ImVec2(Min.x + PadX + IconSize * 0.5f, MidY), IconSize);
		DL->AddText(ImVec2(Min.x + PadX + IconSize + 6.0f * S, MidY - TextSize.y * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), Label);

		if (Tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", Tooltip);
		ImGui::SameLine(0.0f, 2.0f * S);
		return bPressed;
	}

	bool ToggleButton(const char* Label, const FIconDraw& DrawIcon, bool& bValue, const char* Tooltip = nullptr)
	{
		const bool bClicked = ToolbarButton(Label, DrawIcon, bValue, Tooltip);
		if (bClicked) bValue = !bValue;
		return bClicked;
	}

	void ToolbarSeparator()
	{
		const float S = UIScale();
		const ImVec2 P = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddLine(ImVec2(P.x + 4.0f * S, P.y + 5.0f * S), ImVec2(P.x + 4.0f * S, P.y + 23.0f * S),
			ImGui::GetColorU32(ImGuiCol_Border), 1.0f);
		ImGui::Dummy(ImVec2(8.0f * S, 28.0f * S));
		ImGui::SameLine(0.0f, 2.0f * S);
	}
}

void FParticleSystemEditorTab::DrawToolbar()
{
	const float S = UIScale();
	ImGui::Dummy(ImVec2(0.0f, 2.0f * S));
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f * S);

	ImGui::BeginDisabled(!ParticleEditor.IsDirty());
	if (ToolbarButton("Save", EngineIcon(EEditorIcon::Save), false, "Save (Ctrl+S)")) ParticleEditor.Save();
	ImGui::EndDisabled();
	ToolbarSeparator();

	if (ToolbarButton("Restart Sim", EngineIcon(EEditorIcon::Reset), false, "Restart the preview (Space)")) ParticleEditor.RestartPreview();
	ToolbarSeparator();

	ToggleButton("Bounds", DrawBoundsIcon, ParticleEditor.bShowBounds, "Show particle bounds");
	ToggleButton("Origin Axis", DrawAxisIcon, ParticleEditor.bShowOriginAxis, "Show origin axis");
	ToolbarSeparator();

	bool bTest = ParticleEditor.IsShowingTestData();      // 이 엔진 전용: 1번 코어 전 렌더 확인
	if (ToggleButton("Test Data", DrawTestDataIcon, bTest, "Render fixed test particles (renderer check)")) ParticleEditor.SetShowTestData(bTest);

	ImGui::NewLine();
	ImGui::Dummy(ImVec2(0.0f, 2.0f * S));
	ImGui::Separator();
}
