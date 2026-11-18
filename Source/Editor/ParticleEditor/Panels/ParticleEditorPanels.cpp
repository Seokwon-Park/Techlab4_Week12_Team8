#include "EnginePCH.h"
#include "ParticleEditorPanels.h"
#include "Editor/ParticleEditor/ParticleSystemEditor.h"
#include "Editor/Details/PropertyEditor.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "UObject/UObjectHash.h"

#include <functional>

namespace
{
	constexpr float EmitterColumnWidth = 210.0f;
	constexpr float HeaderHeight = 58.0f;

	// Cascade 색. 선택은 주황, 머리는 밝은 회색
	constexpr ImU32 ColorSelected = IM_COL32(240, 122, 26, 255);
	constexpr ImU32 ColorHeader = IM_COL32(201, 204, 210, 255);
	constexpr ImU32 ColorRequired = IM_COL32(168, 169, 92, 255);
	constexpr ImU32 ColorSpawn = IM_COL32(184, 96, 92, 255);
	constexpr ImU32 ColorSubImage = IM_COL32(61, 122, 61, 255);
	constexpr ImU32 ColorLight = IM_COL32(122, 42, 42, 255);
	constexpr ImU32 ColorDefault = IM_COL32(51, 55, 62, 255);
	constexpr ImU32 ColorDisabledTx = IM_COL32(150, 150, 150, 255);

	// "UParticleModuleLifetime" → "Lifetime"
	FString ModuleLabel(const UClass* Class)
	{
		FString Name = Class ? Class->Name : FString("None");
		const FString Prefix = "UParticleModule";
		if (Name.rfind(Prefix, 0) == 0 && Name.size() > Prefix.size()) Name = Name.substr(Prefix.size());
		return Name;
	}

	// UE는 모듈마다 ModuleEditorColor를 갖는다. 여기선 이름으로 고른다 (2번이 모듈에 색 필드를 두면 그쪽으로)
	ImU32 ModuleColor(const UParticleModule* Module)
	{
		if (Module->IsA<UParticleModuleRequired>()) return ColorRequired;
		const FString& Name = Module->GetClass()->Name;
		if (Name.find("Spawn") != FString::npos) return ColorSpawn;
		if (Name.find("SubUV") != FString::npos || Name.find("SubImage") != FString::npos) return ColorSubImage;
		if (Name.find("Light") != FString::npos) return ColorLight;
		return ColorDefault;
	}

	struct FBarResult
	{
		bool bClicked = false;
		bool bRightClicked = false;
		bool bToggled = false;
		bool bCurveClicked = false;     // 추가
	};

	// 색 막대 한 줄: [이름 ............ ☑]. bEnabled가 nullptr이면 체크박스 없음 (Required)
	FBarResult ModuleBar(const char* Id, const char* Label, ImU32 Color, bool* bEnabled, const bool* bCurveOn = nullptr)
	{
		FBarResult Result;
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 Min = ImGui::GetCursorScreenPos();
		const float Width = ImGui::GetContentRegionAvail().x;
		const float Height = ImGui::GetFrameHeight();

		DrawList->AddRectFilled(Min, ImVec2(Min.x + Width, Min.y + Height), Color);

		ImGui::SetNextItemAllowOverlap();
		Result.bClicked = ImGui::InvisibleButton(Id, ImVec2(Width, Height));
		Result.bRightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

		const bool bOff = bEnabled && !*bEnabled;
		DrawList->AddText(ImVec2(Min.x + 6.0f, Min.y + (Height - ImGui::GetTextLineHeight()) * 0.5f),
			bOff ? ColorDisabledTx : IM_COL32_WHITE, Label);

		if (bEnabled)
		{
			ImGui::SetCursorScreenPos(ImVec2(Min.x + Width - Height, Min.y));
			ImGui::PushID(Id);
			Result.bToggled = ImGui::Checkbox("##On", bEnabled);
			ImGui::PopID();
		}

		if (bCurveOn)
		{
			ImGui::SetCursorScreenPos(ImVec2(Min.x + Width - Height * 2.0f - 2.0f, Min.y));
			ImGui::PushID(Id);
			// Cascade의 커브 버튼: 작은 그래프 상자 안에 곡선. 켜져 있으면 초록 바탕
			const ImVec2 BMin = ImGui::GetCursorScreenPos();
			Result.bCurveClicked = ImGui::InvisibleButton("##Curve", ImVec2(Height, Height));
			const bool bHov = ImGui::IsItemHovered();
			const float Pad = Height * 0.18f;
			const ImVec2 A(BMin.x + Pad, BMin.y + Pad), B(BMin.x + Height - Pad, BMin.y + Height - Pad);
			const ImU32 BoxBg = *bCurveOn ? IM_COL32(60, 170, 70, 255) : bHov ? IM_COL32(0, 0, 0, 110) : IM_COL32(0, 0, 0, 70);
			const ImU32 Line = *bCurveOn || bHov ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 205, 200);
			DrawList->AddRectFilled(A, B, BoxBg, 2.0f);
			DrawList->AddRect(A, B, Line, 2.0f, 0, 1.0f);
			const float W = B.x - A.x, H = B.y - A.y;
			DrawList->AddBezierCubic(ImVec2(A.x + W * 0.12f, A.y + H * 0.8f), ImVec2(A.x + W * 0.45f, A.y + H * 0.85f),
				ImVec2(A.x + W * 0.5f, A.y + H * 0.15f), ImVec2(A.x + W * 0.88f, A.y + H * 0.22f), Line, 1.4f);
			if (bHov) ImGui::SetTooltip("Show in Curve Editor");
			ImGui::PopID();
		}

		// 다음 줄 위치. SetCursorScreenPos 뒤에는 Dummy로 줄을 확정한다 (ImGui가 경계 확장을 요구)
		ImGui::SetCursorScreenPos(ImVec2(Min.x, Min.y + Height + 1.0f));
		ImGui::Dummy(ImVec2(0.0f, 0.0f));
		return Result;
	}

	constexpr ImU32 ChannelColors[3] = { IM_COL32(235, 80, 80, 255), IM_COL32(90, 210, 90, 255), IM_COL32(90, 140, 255, 255) };
	constexpr ImU32 FloatCurveColor = IM_COL32(240, 200, 80, 255);

	ImU32 CurveColor(int32 NumChannels, int32 Channel)
	{
		return NumChannels == 1 ? FloatCurveColor : ChannelColors[Channel];
	}

	// 격자 간격: 범위를 약 8칸으로 나누는 1·2·5 × 10^n
	float NiceStep(float Range)
	{
		const float Raw = std::max(Range, 1e-4f) / 8.0f;
		const float Pow10 = std::pow(10.0f, std::floor(std::log10(Raw)));
		const float N = Raw / Pow10;
		return (N < 1.5f ? 1.0f : N < 3.5f ? 2.0f : N < 7.5f ? 5.0f : 10.0f) * Pow10;
	}

	// ───────── 커브 에디터 툴바 (Cascade 모양: 위에 그림, 아래에 글자) ─────────
	// 아이콘 파일 없이 선·점으로 직접 그린다. 공용 EditorIcons를 건드리지 않기 위해서다
	enum class ECurveTool : uint8
	{
		FitH, FitV, Fit, Pan, Zoom,
		Auto, User, Linear, Constant,
		Flatten, Straighten, ShowAll, Delete,
	};

	float UIScale() { return ImGui::GetFontSize() / 13.0f; }

	// BeginDisabled 안이면 흐리게 (DrawList로 직접 그리는 색은 자동으로 안 흐려진다)
	ImU32 WithStyleAlpha(ImU32 Color)
	{
		const float A = ImGui::GetStyle().Alpha;
		if (A >= 1.0f) return Color;
		const uint32 Alpha = static_cast<uint32>(((Color >> IM_COL32_A_SHIFT) & 0xFF) * A);
		return (Color & ~IM_COL32_A_MASK) | (Alpha << IM_COL32_A_SHIFT);
	}

	void DrawArrow(ImDrawList* DL, ImVec2 From, ImVec2 To, ImU32 Color, float Thickness, float HeadSize)
	{
		ImVec2 D(To.x - From.x, To.y - From.y);
		const float Len = std::sqrt(D.x * D.x + D.y * D.y);
		if (Len < 1e-3f) return;
		D.x /= Len; D.y /= Len;
		const ImVec2 N(-D.y, D.x);
		const ImVec2 Base(To.x - D.x * HeadSize * 1.4f, To.y - D.y * HeadSize * 1.4f);
		DL->AddLine(From, Base, Color, Thickness);
		DL->AddTriangleFilled(To, ImVec2(Base.x + N.x * HeadSize, Base.y + N.y * HeadSize),
			ImVec2(Base.x - N.x * HeadSize, Base.y - N.y * HeadSize), Color);
	}

	// C를 중심으로 반지름 S 정사각형 안에 그린다. 좌표는 -1 ~ 1
	void DrawToolIcon(ImDrawList* DL, ECurveTool Tool, ImVec2 C, float S)
	{
		const ImU32 Line = WithStyleAlpha(IM_COL32(205, 208, 215, 255));
		const ImU32 CurveCol = WithStyleAlpha(IM_COL32(120, 160, 235, 255));
		const ImU32 KeyCol = WithStyleAlpha(IM_COL32(220, 64, 58, 255));
		const float Th = std::max(1.5f, S * 0.13f);
		const float Head = S * 0.24f;
		auto P = [&](float X, float Y) { return ImVec2(C.x + X * S, C.y + Y * S); };
		auto Key = [&](float X, float Y)
			{
				const ImVec2 Q = P(X, Y);
				const float H = S * 0.18f;
				DL->AddRectFilled(ImVec2(Q.x - H, Q.y - H), ImVec2(Q.x + H, Q.y + H), KeyCol);
			};
		auto Handle = [&](float X, float Y) { DL->AddCircleFilled(P(X, Y), S * 0.11f, Line); };

		switch (Tool)
		{
		case ECurveTool::FitH:
			DL->AddLine(P(-0.95f, -0.8f), P(-0.95f, 0.8f), Line, Th);
			DL->AddLine(P(0.95f, -0.8f), P(0.95f, 0.8f), Line, Th);
			DrawArrow(DL, P(0.0f, 0.0f), P(-0.75f, 0.0f), Line, Th, Head);
			DrawArrow(DL, P(0.0f, 0.0f), P(0.75f, 0.0f), Line, Th, Head);
			break;
		case ECurveTool::FitV:
			DL->AddLine(P(-0.8f, -0.95f), P(0.8f, -0.95f), Line, Th);
			DL->AddLine(P(-0.8f, 0.95f), P(0.8f, 0.95f), Line, Th);
			DrawArrow(DL, P(0.0f, 0.0f), P(0.0f, -0.75f), Line, Th, Head);
			DrawArrow(DL, P(0.0f, 0.0f), P(0.0f, 0.75f), Line, Th, Head);
			break;
		case ECurveTool::Fit:
			DL->AddRect(P(-0.95f, -0.95f), P(0.95f, 0.95f), Line, 0.0f, 0, Th * 0.7f);
			DL->AddLine(P(-0.55f, 0.5f), P(0.0f, -0.05f), CurveCol, Th);
			DL->AddLine(P(0.0f, -0.05f), P(0.55f, -0.5f), CurveCol, Th);
			Key(-0.55f, 0.5f); Key(0.0f, -0.05f); Key(0.55f, -0.5f);
			break;
		case ECurveTool::Pan:
			DrawArrow(DL, P(0.0f, 0.0f), P(-0.95f, 0.0f), Line, Th, Head);
			DrawArrow(DL, P(0.0f, 0.0f), P(0.95f, 0.0f), Line, Th, Head);
			DrawArrow(DL, P(0.0f, 0.0f), P(0.0f, -0.95f), Line, Th, Head);
			DrawArrow(DL, P(0.0f, 0.0f), P(0.0f, 0.95f), Line, Th, Head);
			break;
		case ECurveTool::Zoom:
			DL->AddCircle(P(-0.2f, -0.2f), S * 0.55f, Line, 0, Th);
			DL->AddLine(P(0.2f, 0.2f), P(0.85f, 0.85f), Line, Th * 1.8f);
			break;
		case ECurveTool::Auto:
			DL->AddBezierCubic(P(-0.95f, 0.6f), P(-0.4f, -0.6f), P(0.4f, -0.6f), P(0.95f, 0.6f), CurveCol, Th);
			DL->AddLine(P(-0.55f, -0.3f), P(0.55f, -0.3f), Line, Th * 0.7f);
			Handle(-0.55f, -0.3f); Handle(0.55f, -0.3f); Key(0.0f, -0.3f);
			break;
		case ECurveTool::User:
			DL->AddBezierCubic(P(-0.95f, 0.6f), P(-0.4f, -0.6f), P(0.4f, -0.6f), P(0.95f, 0.6f), CurveCol, Th);
			DL->AddLine(P(-0.5f, -0.05f), P(0.5f, -0.55f), Line, Th * 0.7f);
			Handle(-0.5f, -0.05f); Handle(0.5f, -0.55f); Key(0.0f, -0.3f);
			break;
		case ECurveTool::Linear:
			DL->AddLine(P(-0.9f, 0.6f), P(0.0f, -0.5f), CurveCol, Th);
			DL->AddLine(P(0.0f, -0.5f), P(0.9f, 0.4f), CurveCol, Th);
			Key(-0.9f, 0.6f); Key(0.0f, -0.5f); Key(0.9f, 0.4f);
			break;
		case ECurveTool::Constant:
			DL->AddLine(P(-0.9f, 0.5f), P(0.1f, 0.5f), CurveCol, Th);
			DL->AddLine(P(0.1f, 0.5f), P(0.1f, -0.5f), CurveCol, Th);
			DL->AddLine(P(0.1f, -0.5f), P(0.9f, -0.5f), CurveCol, Th);
			Key(-0.9f, 0.5f); Key(0.1f, -0.5f);
			break;
		case ECurveTool::Flatten:
			DrawArrow(DL, P(-0.55f, -0.95f), P(-0.55f, 0.05f), Line, Th, Head);
			DrawArrow(DL, P(0.55f, -0.95f), P(0.55f, 0.05f), Line, Th, Head);
			DL->AddLine(P(-0.9f, 0.45f), P(0.9f, 0.45f), Line, Th * 0.7f);
			Handle(-0.9f, 0.45f); Handle(0.9f, 0.45f); Key(0.0f, 0.45f);
			break;
		case ECurveTool::Straighten:
			DL->AddLine(P(-0.85f, 0.65f), P(0.85f, -0.45f), Line, Th * 0.7f);
			DrawArrow(DL, P(-0.75f, -0.75f), P(-0.4f, 0.1f), Line, Th * 0.8f, Head * 0.8f);
			DrawArrow(DL, P(0.75f, 0.75f), P(0.4f, -0.05f), Line, Th * 0.8f, Head * 0.8f);
			Handle(-0.85f, 0.65f); Handle(0.85f, -0.45f); Key(0.0f, 0.1f);
			break;
		case ECurveTool::ShowAll:
			for (int32 i = 0; i < 3; ++i)
			{
				const float Y = -0.5f + i * 0.5f;
				DL->AddBezierCubic(P(-0.95f, Y + 0.25f), P(-0.3f, Y - 0.35f), P(0.3f, Y + 0.35f), P(0.95f, Y - 0.25f),
					WithStyleAlpha(ChannelColors[i]), Th);
			}
			break;
		case ECurveTool::Delete:
			DL->AddLine(P(-0.95f, 0.5f), P(0.95f, 0.5f), CurveCol, Th);
			Key(0.0f, 0.5f);
			DL->AddLine(P(-0.45f, -0.85f), P(0.45f, 0.05f), KeyCol, Th * 1.3f);
			DL->AddLine(P(0.45f, -0.85f), P(-0.45f, 0.05f), KeyCol, Th * 1.3f);
			break;
		}
	}

	constexpr float ToolbarHeight = 50.0f;   // 배율 1일 때
	constexpr float ToolSpacing = 2.0f;
	constexpr float ToolSeparatorWidth = 8.0f;
	constexpr float OverflowButtonWidth = 34.0f;

	float ToolButtonWidth(const char* Label)
	{
		const float Scale = UIScale();
		return std::max(44.0f * Scale, ImGui::CalcTextSize(Label).x + 12.0f * Scale);
	}

	// 그림 + 글자 버튼. bActive면 주황 바탕 (Cascade의 Pan처럼 지금 켜진 것)
	bool ToolButton(const char* Label, ECurveTool Tool, bool bActive = false, const char* Tooltip = nullptr)
	{
		const float Scale = UIScale();
		const float H = ToolbarHeight * Scale;
		const ImVec2 LabelSize = ImGui::CalcTextSize(Label);
		const float W = ToolButtonWidth(Label);
		const ImVec2 Pos = ImGui::GetCursorScreenPos();

		const bool bClicked = ImGui::InvisibleButton(Label, ImVec2(W, H));
		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = ImGui::IsItemActive();
		ImDrawList* DL = ImGui::GetWindowDrawList();

		if (bActive || bHovered)
		{
			const ImU32 Bg = bActive ? (bHovered ? IM_COL32(255, 145, 55, 255) : ColorSelected)
				: bHeld ? IM_COL32(88, 90, 96, 255) : IM_COL32(64, 66, 72, 255);
			DL->AddRectFilled(Pos, ImVec2(Pos.x + W, Pos.y + H), WithStyleAlpha(Bg), 3.0f * Scale);
		}
		DrawToolIcon(DL, Tool, ImVec2(Pos.x + W * 0.5f, Pos.y + 18.0f * Scale), 12.0f * Scale);
		DL->AddText(ImVec2(Pos.x + (W - LabelSize.x) * 0.5f, Pos.y + H - LabelSize.y - 4.0f * Scale),
			WithStyleAlpha(IM_COL32(222, 224, 230, 255)), Label);

		if (Tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", Tooltip);
		ImGui::SameLine(0.0f, 2.0f * Scale);
		return bClicked;
	}

	void ToolSeparator()
	{
		const float Scale = UIScale();
		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		const float H = ToolbarHeight * Scale;
		ImGui::GetWindowDrawList()->AddLine(ImVec2(Pos.x + 4.0f * Scale, Pos.y + 6.0f * Scale),
			ImVec2(Pos.x + 4.0f * Scale, Pos.y + H - 6.0f * Scale), IM_COL32(78, 80, 86, 255), 1.0f);
		ImGui::Dummy(ImVec2(8.0f * Scale, H));
		ImGui::SameLine(0.0f, 2.0f * Scale);
	}

	// "UParticleModuleColorOverLife" → "ColorOverLife"
	FString ShortModuleName(const UParticleModule* Module)
	{
		FString Name = Module ? Module->GetClass()->Name : FString{};
		const FString Prefix = "UParticleModule";
		if (Name.rfind(Prefix, 0) == 0 && Name.size() > Prefix.size()) Name = Name.substr(Prefix.size());
		return Name;
	}
}

FParticleEmittersPanel::FParticleEmittersPanel(FParticleSystemEditor& InEditor)
	: FParticleEditorPanel(InEditor, "Emitters", ParticlePanelIdPrefix)
{
}

void FParticleEmittersPanel::DrawContent()
{
	Editor.HandleShortcuts();
	UParticleSystem* System = Editor.GetSystem();
	if (!System) return;

	ImGui::BeginChild("##EmitterStrip", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
	{
		const int32 Count = static_cast<int32>(System->Emitters.Num());
		for (int32 i = 0; i < Count; ++i)
		{
			if (i > 0) ImGui::SameLine(0.0f, 2.0f);
			DrawEmitterColumn(i);
		}
		if (Count == 0) ImGui::TextDisabled("Right-click to add an emitter.");

		if (ImGui::BeginPopupContextWindow("##StripMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("New Emitter")) Editor.RequestAddEmitter();
			ImGui::EndPopup();
		}
	}
	ImGui::EndChild();
}

void FParticleEmittersPanel::DrawEmitterColumn(int32 EmitterIndex)
{
	UParticleEmitter* Emitter = Editor.GetSystem()->Emitters[EmitterIndex];
	UParticleLODLevel* LOD = Editor.GetLOD(EmitterIndex);

	ImGui::PushID(EmitterIndex);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 1.0f));
	ImGui::BeginChild("##Emitter", ImVec2(EmitterColumnWidth, 0), ImGuiChildFlags_None);

	DrawEmitterHeader(EmitterIndex, Emitter, LOD);

	if (LOD)
	{
		// ── TypeData: 머리 바로 아래 글자만 (Cascade의 "GPU Sprites") ──
		if (UParticleModuleTypeDataBase* TypeData = LOD->TypeDataModule)
		{
			const FString Label = ModuleLabel(TypeData->GetClass());
			const bool bSelected = Editor.GetSelectedObject() == TypeData;
			const FBarResult Bar = ModuleBar("##TypeData", Label.c_str(), bSelected ? ColorSelected : IM_COL32(0, 0, 0, 0), nullptr);
			if (Bar.bClicked) Editor.Select(EmitterIndex, TypeData);
			if (Bar.bRightClicked) ImGui::OpenPopup("##TypeDataMenu");
			if (ImGui::BeginPopup("##TypeDataMenu"))
			{
				if (ImGui::MenuItem("Remove Type Data")) Editor.RequestSetTypeData(EmitterIndex, nullptr);
				ImGui::EndPopup();
			}
		}

		// ── Required: 끄거나 지울 수 없다 ──
		if (UParticleModuleRequired* Required = LOD->RequiredModule)
		{
			const bool bSelected = Editor.GetSelectedObject() == Required;
			if (ModuleBar("##Required", "Required", bSelected ? ColorSelected : ColorRequired, nullptr).bClicked)
				Editor.Select(EmitterIndex, Required);
		}

		// ── 모듈: 위에서 아래가 실행 순서 ──
		for (int32 i = 0; i < static_cast<int32>(LOD->Modules.Num()); ++i)
		{
			DrawModuleRow(EmitterIndex, LOD, LOD->Modules[i], i);
		}

		// 열 안의 빈 곳
		if (ImGui::BeginPopupContextWindow("##AddModule", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			DrawAddModuleMenu(EmitterIndex, LOD);
			ImGui::EndPopup();
		}
	}

	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopID();
}

void FParticleEmittersPanel::DrawEmitterHeader(int32 EmitterIndex, UParticleEmitter* Emitter, UParticleLODLevel* LOD)
{
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const ImVec2 Min = ImGui::GetCursorScreenPos();
	const float Width = ImGui::GetContentRegionAvail().x;
	const bool bSelected = Editor.GetSelectedObject() == Emitter;
	const ImU32 TextColor = bSelected ? IM_COL32_WHITE : IM_COL32(26, 29, 34, 255);

	// 배경 + 클릭 영역
	DrawList->AddRectFilled(Min, ImVec2(Min.x + Width, Min.y + HeaderHeight), bSelected ? ColorSelected : ColorHeader);
	ImGui::SetNextItemAllowOverlap();
	if (ImGui::InvisibleButton("##Header", ImVec2(Width, HeaderHeight))) Editor.Select(EmitterIndex, Emitter);
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("##EmitterMenu");

	// 1줄: 이름
	const char* Name = Emitter ? Emitter->EmitterName.c_str() : "(null)";
	DrawList->AddText(ImVec2(Min.x + 6.0f, Min.y + 4.0f), TextColor, Name);

	// 2줄: 켜기 체크박스 + 활성 개수
	if (LOD)
	{
		ImGui::SetCursorScreenPos(ImVec2(Min.x + 4.0f, Min.y + HeaderHeight - ImGui::GetFrameHeight() - 4.0f));
		bool bOn = LOD->bEnabled;
		if (ImGui::Checkbox("##On", &bOn))
		{
			LOD->bEnabled = bOn;
			Editor.NotifyPropertyChanged();
		}
	}
	char Count[16];
	snprintf(Count, sizeof(Count), "%d", Editor.GetActiveParticles(EmitterIndex));
	DrawList->AddText(ImVec2(Min.x + Width - HeaderHeight - 8.0f - ImGui::CalcTextSize(Count).x,
		Min.y + HeaderHeight - ImGui::GetTextLineHeight() - 8.0f), TextColor, Count);

	// 오른쪽: 텍스처 썸네일 (Required.Texture)
	const float Thumb = HeaderHeight - 8.0f;
	const ImVec2 ThumbMin(Min.x + Width - Thumb - 4.0f, Min.y + 4.0f);
	UTexture2D* Texture = (LOD && LOD->RequiredModule) ? LOD->RequiredModule->Texture : nullptr;
	if (Texture && Texture->GetResource() && Texture->GetResource()->GetSRV())
	{
		DrawList->AddImage((ImTextureID)Texture->GetResource()->GetSRV(), ThumbMin, ImVec2(ThumbMin.x + Thumb, ThumbMin.y + Thumb));
	}
	else
	{
		DrawList->AddRectFilled(ThumbMin, ImVec2(ThumbMin.x + Thumb, ThumbMin.y + Thumb), IM_COL32(0, 0, 0, 255));
	}

	if (ImGui::BeginPopup("##EmitterMenu"))
	{
		if (ImGui::MenuItem("Delete Emitter")) Editor.RequestRemoveEmitter(EmitterIndex);
		ImGui::EndPopup();
	}

	ImGui::SetCursorScreenPos(ImVec2(Min.x, Min.y + HeaderHeight + 4.0f));
	ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

void FParticleEmittersPanel::DrawModuleRow(int32 EmitterIndex, UParticleLODLevel* LOD, UParticleModule* Module, int32 ModuleIndex)
{
	if (!Module) return;
	ImGui::PushID(Module);

	const FString Label = ModuleLabel(Module->GetClass());
	const bool bSelected = Editor.GetSelectedObject() == Module;
	bool bOn = Module->bEnabled;

	const bool bCurveOn = Editor.HasCurveTracks(Module);
	const bool bHasCurves = FParticleSystemEditor::HasDistributions(Module);
	const FBarResult Bar = ModuleBar("##Row", Label.c_str(), bSelected ? ColorSelected : ModuleColor(Module), &bOn,
		bHasCurves ? &bCurveOn : nullptr);

	if (Bar.bClicked) Editor.Select(EmitterIndex, Module);
	if (Bar.bToggled)
	{
		Module->bEnabled = bOn;
		Editor.NotifyPropertyChanged();
	}
	if (Bar.bRightClicked) ImGui::OpenPopup("##ModuleMenu");
	if (Bar.bCurveClicked) Editor.ToggleCurveTracks(Module);

	if (ImGui::BeginPopup("##ModuleMenu"))
	{
		const int32 Num = static_cast<int32>(LOD->Modules.Num());
		if (ImGui::MenuItem("Move Up", nullptr, false, ModuleIndex > 0))
			Editor.RequestMoveModule(EmitterIndex, ModuleIndex, ModuleIndex - 1);
		if (ImGui::MenuItem("Move Down", nullptr, false, ModuleIndex + 1 < Num))
			Editor.RequestMoveModule(EmitterIndex, ModuleIndex, ModuleIndex + 1);
		ImGui::Separator();
		if (ImGui::MenuItem("Delete Module", "Del")) Editor.RequestRemoveModule(EmitterIndex, Module);
		ImGui::EndPopup();
	}
	ImGui::PopID();
}

void FParticleEmittersPanel::DrawAddModuleMenu(int32 EmitterIndex, UParticleLODLevel* LOD)
{
	TArray<UClass*> Classes;
	GetDerivedClasses(UParticleModule::StaticClass(), Classes);

	UClass* const TypeDataBase = UParticleModuleTypeDataBase::StaticClass();

	ImGui::TextDisabled("Add Module");
	for (UClass* Class : Classes)
	{
		// 생성 불가, 베이스, Required(항상 하나), TypeData(아래 메뉴)는 뺀다
		if (!Class->Constructor || Class == UParticleModule::StaticClass() ||
			Class == UParticleModuleRequired::StaticClass() || Class->IsChildOf(TypeDataBase))
			continue;

		bool bAlreadyHas = false;   // UE처럼 같은 모듈은 한 번만
		for (UParticleModule* Existing : LOD->Modules)
			if (Existing && Existing->GetClass() == Class) { bAlreadyHas = true; break; }

		if (ImGui::MenuItem(ModuleLabel(Class).c_str(), nullptr, false, !bAlreadyHas))
			Editor.RequestAddModule(EmitterIndex, Class);
	}

	if (ImGui::BeginMenu("Type Data"))
	{
		if (ImGui::MenuItem("Sprite (none)", nullptr, LOD->TypeDataModule == nullptr))
			Editor.RequestSetTypeData(EmitterIndex, nullptr);
		for (UClass* Class : Classes)
		{
			if (!Class->Constructor || Class == TypeDataBase || !Class->IsChildOf(TypeDataBase)) continue;
			const bool bCurrent = LOD->TypeDataModule && LOD->TypeDataModule->GetClass() == Class;
			if (ImGui::MenuItem(ModuleLabel(Class).c_str(), nullptr, bCurrent))
				Editor.RequestSetTypeData(EmitterIndex, Class);
		}
		ImGui::EndMenu();
	}
}

////////////////////////////////////////////////////////////
// Viewport
////////////////////////////////////////////////////////////

FParticleViewportPanel::FParticleViewportPanel(FParticleSystemEditor& InEditor)
	: FParticleEditorPanel(InEditor, "Viewport", ParticlePanelIdPrefix)
{
}

void FParticleViewportPanel::DrawContent()
{
	Editor.HandleShortcuts();

	FEditorViewportClient* Client = Editor.GetViewportClient();
	const ImVec2 Size = ImGui::GetContentRegionAvail();
	if (!Client || Size.x < 1.0f || Size.y < 1.0f) return;

	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##ParticleViewport", Size,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	if (FTexture2D* Target = Client->Viewport ? Client->Viewport->GetRenderTargetTexture() : nullptr; Target && Target->GetSRV())
	{
		ImGui::GetWindowDrawList()->AddImage(Target->GetSRV(), Origin, ImVec2(Origin.x + Size.x, Origin.y + Size.y));
	}

	// Cascade처럼 오른쪽 아래에 이미터별 "활성 / 최대" 개수. 위에서부터 이미터 순서
	if (UParticleSystem* System = Editor.GetSystem())
	{
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const int32 Count = static_cast<int32>(System->Emitters.Num());
		const float LineHeight = ImGui::GetTextLineHeight(); 
		float Y = Origin.y + Size.y - 6.0f - LineHeight * Count;
		for (int32 i = 0; i < Count; ++i, Y += LineHeight)
		{
			char Text[32];
			snprintf(Text, sizeof(Text), "%d / %d", Editor.GetActiveParticles(i), Editor.GetMaxActiveParticles(i));
			const float Width = ImGui::CalcTextSize(Text).x;
			DrawList->AddText(ImVec2(Origin.x + Size.x - 8.0f - Width, Y), IM_COL32(230, 145, 60, 255), Text);
		}
	}

	const bool bHovered = ImGui::IsItemHovered();
	if (bHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
	{
		ImGui::SetWindowFocus();
	}
	const bool bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (bFocused && bHovered && ImGui::IsKeyPressed(ImGuiKey_F, false))
	{
		Editor.FocusPreview();
	}

	const ImGuiIO& IO = ImGui::GetIO();
	FViewportCameraInput Input
	{
		.bCaptured = bFocused && ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Right),
		.bAllowWheel = bHovered,
		.bForward = ImGui::IsKeyDown(ImGuiKey_W),
		.bBackward = ImGui::IsKeyDown(ImGuiKey_S),
		.bRight = ImGui::IsKeyDown(ImGuiKey_D),
		.bLeft = ImGui::IsKeyDown(ImGuiKey_A),
		.bUp = ImGui::IsKeyDown(ImGuiKey_E),
		.bDown = ImGui::IsKeyDown(ImGuiKey_Q),
		.MouseX = IO.MouseDelta.x,
		.MouseY = IO.MouseDelta.y,
		.Wheel = IO.MouseWheel * 120.0f,
	};
	Editor.SetViewportState(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y), Input);
}

namespace
{
	// 선택 객체의 UPROPERTY 바이트를 이어 붙인다. 그리기 전후를 비교해 값 변경을 안다
	TArray<uint8> SnapshotProperties(UObject* Object)
	{
		TArray<uint8> Bytes;
		const uint8* Base = reinterpret_cast<const uint8*>(Object);
		for (const UStruct* Struct = Object->GetClass(); Struct; Struct = Struct->GetSuperStruct())
		{
			for (const FProperty* Property : Struct->GetProperties())
			{
				const uint8* Value = Base + Property->Offset;
				for (uint32 i = 0; i < Property->ElementSize; ++i) Bytes.Add(Value[i]);
			}
		}
		return Bytes;
	}

	bool SameBytes(const TArray<uint8>& A, const TArray<uint8>& B)
	{
		return A.Num() == B.Num() && (A.Num() == 0 || std::memcmp(A.GetData(), B.GetData(), A.Num()) == 0);
	}
}

FParticleDetailsPanel::FParticleDetailsPanel(FParticleSystemEditor& InEditor)
	: FParticleEditorPanel(InEditor, "Details", ParticlePanelIdPrefix)
{
}

void FParticleDetailsPanel::DrawContent()
{
	UObject* Selected = Editor.GetSelectedObject();
	if (!Selected)
	{
		ImGui::TextDisabled("Select an emitter or a module.");
		return;
	}

	ImGui::TextDisabled("%s", Selected->GetClass()->Name.c_str());
	ImGui::Separator();

	const TArray<uint8> Before = SnapshotProperties(Selected);
	FPropertyEditor::DrawObjectProperties(Selected);          // 레벨 디테일 패널과 같은 위젯 (텍스처 피커 포함)
	if (!SameBytes(Before, SnapshotProperties(Selected)))
	{
		Editor.NotifyPropertyChanged();
	}
}

FParticleCurveEditorPanel::FParticleCurveEditorPanel(FParticleSystemEditor& InEditor)
	: FParticleEditorPanel(InEditor, "Curve Editor", ParticlePanelIdPrefix)
{
}

FParticleCurve* FParticleCurveEditorPanel::GetSelectedCurve() const
{
	TArray<FParticleSystemEditor::FCurveTrack>& Tracks = Editor.GetCurveTracks();
	if (SelectedTrack < 0 || SelectedTrack >= static_cast<int32>(Tracks.Num())) return nullptr;
	return Tracks[SelectedTrack].GetChannel(SelectedChannel);
}

void FParticleCurveEditorPanel::DrawContent()
{
	// 트랙 목록이 바뀌었으면(모듈 삭제 등) 선택을 버린다
	if (SelectedTrack >= static_cast<int32>(Editor.GetCurveTracks().Num())) { SelectedTrack = -1; SelectedKey = -1; }

	DrawToolbar();

	const float Scale = UIScale();
	ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(118, 118, 122, 255));     // Cascade의 회색 트랙 영역
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f * Scale, 4.0f * Scale));
	ImGui::BeginChild("##Tracks", ImVec2(200.0f * Scale, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);
	DrawTrackList();
	ImGui::EndChild();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();

	ImGui::SameLine(0.0f, 0.0f);
	DrawGraph();
}

void FParticleCurveEditorPanel::DrawToolbar()
{
	const float Scale = UIScale();
	const float BarH = (ToolbarHeight + 6.0f) * Scale;
	const ImVec2 Start = ImGui::GetCursorScreenPos();
	const float BarW = ImGui::GetContentRegionAvail().x;
	ImGui::GetWindowDrawList()->AddRectFilled(Start, ImVec2(Start.x + BarW, Start.y + BarH), IM_COL32(30, 31, 34, 255));

	FParticleCurve* Curve = GetSelectedCurve();
	const bool bHasKey = Curve && SelectedKey >= 0 && SelectedKey < static_cast<int32>(Curve->Keys.Num());
	const FParticleCurveKey* Key = bHasKey ? &Curve->Keys[SelectedKey] : nullptr;
	const bool bCubic = Key && Key->InterpMode == EParticleCurveInterp::Cubic;
	auto Apply = [&](auto&& Edit)
		{
			Edit(Curve->Keys[SelectedKey]);
			Curve->AutoSetTangents();
			Editor.NotifyPropertyChanged();
		};

	// ── 버튼 목록 ── 한 번 만들어 두고, 툴바에 들어가는 만큼만 그리고 나머지는 ≡ 메뉴로 보낸다 (UE 툴바처럼)
	struct FToolItem
	{
		const char* Label;
		ECurveTool Tool;
		bool bActive;
		bool bEnabled;
		bool bSeparatorBefore;     // 앞에 구분선
		const char* Tooltip;
		std::function<void()> OnClick;
	};
	const FToolItem Items[] = {
		{ "Horizontal", ECurveTool::FitH, false, true, false, "Fit time range to visible keys", [&] { FitHorizontal(); } },
		{ "Vertical", ECurveTool::FitV, false, true, false, "Fit value range to visible curves", [&] { FitVertical(); } },
		{ "Fit", ECurveTool::Fit, false, true, false, "Fit both", [&] { FitHorizontal(); FitVertical(); } },
		{ "Pan", ECurveTool::Pan, !bZoomMode, true, true, "Left-drag on empty space pans", [&] { bZoomMode = false; } },
		{ "Zoom", ECurveTool::Zoom, bZoomMode, true, false, "Left-drag on empty space zooms (right/up = in)", [&] { bZoomMode = true; } },

		{ "Auto", ECurveTool::Auto, bCubic && Key->bAutoTangent, bHasKey, true, "Smooth curve, tangents computed from neighbours",
			[&] { Apply([](FParticleCurveKey& K) { K.InterpMode = EParticleCurveInterp::Cubic; K.bAutoTangent = true; }); } },
		{ "User", ECurveTool::User, bCubic && !Key->bAutoTangent, bHasKey, false, "Smooth curve, keep current tangents",
			[&] { Apply([](FParticleCurveKey& K) { K.InterpMode = EParticleCurveInterp::Cubic; K.bAutoTangent = false; }); } },
		{ "Linear", ECurveTool::Linear, Key && Key->InterpMode == EParticleCurveInterp::Linear, bHasKey, false, "Straight line to next key",
			[&] { Apply([](FParticleCurveKey& K) { K.InterpMode = EParticleCurveInterp::Linear; }); } },
		{ "Constant", ECurveTool::Constant, Key && Key->InterpMode == EParticleCurveInterp::Constant, bHasKey, false, "Hold value until next key",
			[&] { Apply([](FParticleCurveKey& K) { K.InterpMode = EParticleCurveInterp::Constant; }); } },

		{ "Flatten", ECurveTool::Flatten, false, bHasKey, true, "Set tangents to 0",
			[&] { Apply([](FParticleCurveKey& K)
				{ K.InterpMode = EParticleCurveInterp::Cubic; K.bAutoTangent = false; K.ArriveTangent = K.LeaveTangent = 0.0f; }); } },
		{ "Straighten", ECurveTool::Straighten, false, bHasKey, false, "Make arrive and leave tangents equal",
			[&] { Apply([](FParticleCurveKey& K)
				{
					K.InterpMode = EParticleCurveInterp::Cubic; K.bAutoTangent = false;
					K.ArriveTangent = K.LeaveTangent = (K.ArriveTangent + K.LeaveTangent) * 0.5f;
				}); } },

		{ "Show All", ECurveTool::ShowAll, false, true, true, "Show every channel of every track",
			[&] {
				for (FParticleSystemEditor::FCurveTrack& Track : Editor.GetCurveTracks())
					for (bool& bVisible : Track.bChannelVisible) bVisible = true;
			} },
		{ "Delete", ECurveTool::Delete, false, bHasKey, false, "Delete selected key (Del)",
			[&] { Curve->RemoveKeyAt(SelectedKey); SelectedKey = -1; Editor.NotifyPropertyChanged(); } },
	};
	constexpr int32 NumItems = static_cast<int32>(sizeof(Items) / sizeof(Items[0]));

	// ── 몇 개가 들어가나 ──
	const float Avail = BarW - 6.0f * Scale;
	auto ItemWidth = [&](const FToolItem& Item)
		{
			return (Item.bSeparatorBefore ? (ToolSeparatorWidth + ToolSpacing) * Scale : 0.0f) + ToolButtonWidth(Item.Label) + ToolSpacing * Scale;
		};
	float AllWidth = 0.0f;
	for (const FToolItem& Item : Items) AllWidth += ItemWidth(Item);

	int32 NumVisible = NumItems;
	if (AllWidth > Avail)                       // 다 안 들어가면 ≡ 버튼 자리를 빼고 앞에서부터 채운다
	{
		const float Limit = Avail - OverflowButtonWidth * Scale;
		float Used = 0.0f;
		NumVisible = 0;
		while (NumVisible < NumItems && Used + ItemWidth(Items[NumVisible]) <= Limit) Used += ItemWidth(Items[NumVisible++]);
	}
	const bool bOverflow = NumVisible < NumItems;

	// 오른쪽 키 Time / Value: 키를 골랐을 때만. 버튼이 다 들어가고 남는 자리가 있으면 툴바에, 아니면 ≡ 메뉴 안에
	const float InfoWidth = (ToolSeparatorWidth + ToolSpacing + 110.0f) * Scale
		+ ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize("Value").x;
	const bool bInfoInBar = bHasKey && !bOverflow && AllWidth + InfoWidth <= Avail;
	const bool bInfoInMenu = bHasKey && !bInfoInBar;

	// ── 그리기 ──
	ImGui::SetCursorScreenPos(ImVec2(Start.x + 3.0f * Scale, Start.y + 3.0f * Scale));
	int32 Clicked = -1;
	for (int32 i = 0; i < NumVisible; ++i)
	{
		const FToolItem& Item = Items[i];
		if (Item.bSeparatorBefore) ToolSeparator();
		ImGui::BeginDisabled(!Item.bEnabled);
		if (ToolButton(Item.Label, Item.Tool, Item.bActive, Item.Tooltip)) Clicked = i;
		ImGui::EndDisabled();
	}

	auto DrawKeyInfo = [&]
		{
			if (SelectedKey >= 0)
			{
				FParticleCurveKey& K = Curve->Keys[SelectedKey];
				const int32 Num = static_cast<int32>(Curve->Keys.Num());
				const float MinT = SelectedKey > 0 ? Curve->Keys[SelectedKey - 1].Time + 1e-3f : -1e6f;
				const float MaxT = SelectedKey < Num - 1 ? Curve->Keys[SelectedKey + 1].Time - 1e-3f : 1e6f;

				ImGui::PushItemWidth(110.0f * Scale);
				bool bChanged = ImGui::DragFloat("Time", &K.Time, 0.005f, MinT, MaxT, "%.3f", ImGuiSliderFlags_AlwaysClamp);
				bChanged |= ImGui::DragFloat("Value", &K.Value, 0.01f, 0.0f, 0.0f, "%.3f");
				ImGui::PopItemWidth();
				if (bChanged)
				{
					Curve->AutoSetTangents();
					Editor.NotifyPropertyChanged();
				}
			}
		};

	if (bInfoInBar)
	{
		ToolSeparator();
		ImGui::BeginGroup();
		ImGui::Dummy(ImVec2(0.0f, 2.0f * Scale));
		DrawKeyInfo();
		ImGui::EndGroup();
	}

	// ── ≡ 버튼 (오른쪽 끝) + 잘린 버튼 메뉴 ──
	if (bOverflow || bInfoInMenu)
	{
		const float OW = OverflowButtonWidth * Scale;
		const float OH = 26.0f * Scale;
		const ImVec2 P(Start.x + BarW - OW - 3.0f * Scale, Start.y + (BarH - OH) * 0.5f);
		ImGui::SetCursorScreenPos(P);
		if (ImGui::InvisibleButton("##ToolbarOverflow", ImVec2(OW, OH))) ImGui::OpenPopup("##ToolbarOverflowMenu");
		const bool bHov = ImGui::IsItemHovered() || ImGui::IsPopupOpen("##ToolbarOverflowMenu");

		ImDrawList* DL = ImGui::GetWindowDrawList();
		if (bHov) DL->AddRectFilled(P, ImVec2(P.x + OW, P.y + OH), IM_COL32(64, 66, 72, 255), 3.0f * Scale);
		const ImU32 Col = IM_COL32(205, 208, 215, 255);
		const float LX = P.x + 7.0f * Scale, LW = 12.0f * Scale, CY = P.y + OH * 0.5f;
		for (int32 i = -1; i <= 1; ++i)         // ≡
			DL->AddLine(ImVec2(LX, CY + i * 4.0f * Scale), ImVec2(LX + LW, CY + i * 4.0f * Scale), Col, 1.6f * Scale);
		const float VX = LX + LW + 5.0f * Scale; // ▾
		DL->AddLine(ImVec2(VX, CY - 1.5f * Scale), ImVec2(VX + 3.5f * Scale, CY + 2.0f * Scale), Col, 1.4f * Scale);
		DL->AddLine(ImVec2(VX + 3.5f * Scale, CY + 2.0f * Scale), ImVec2(VX + 7.0f * Scale, CY - 1.5f * Scale), Col, 1.4f * Scale);

		if (ImGui::BeginPopup("##ToolbarOverflowMenu"))
		{
			const float IconSize = ImGui::GetFontSize();
			for (int32 i = NumVisible; i < NumItems; ++i)
			{
				const FToolItem& Item = Items[i];
				if (Item.bSeparatorBefore && i > NumVisible) ImGui::Separator();

				// 메뉴 줄 왼쪽에 툴바와 같은 그림을 작게 그린다
				const ImVec2 RowPos = ImGui::GetCursorScreenPos();
				ImGui::BeginDisabled(!Item.bEnabled);
				ImGui::PushID(i);
				if (ImGui::MenuItem((FString("      ") + Item.Label).c_str(), nullptr, Item.bActive, Item.bEnabled)) Clicked = i;
				ImGui::PopID();
				DrawToolIcon(ImGui::GetWindowDrawList(), Item.Tool,
					ImVec2(RowPos.x + IconSize * 0.6f, RowPos.y + ImGui::GetTextLineHeight() * 0.5f), IconSize * 0.45f);
				ImGui::EndDisabled();
			}

			if (bInfoInMenu)
			{
				if (bOverflow) ImGui::Separator();
				ImGui::TextDisabled("Selected key");
				DrawKeyInfo();
			}
			ImGui::EndPopup();
		}
	}

	if (Clicked >= 0) Items[Clicked].OnClick();   // 그리기가 끝난 뒤 실행 (Delete가 키를 지워도 위 코드는 안전)

	// 툴바 높이만큼 자리 차지 (커서만 옮기면 ImGui가 경고하므로 Dummy로)
	ImGui::SetCursorScreenPos(Start);
	ImGui::Dummy(ImVec2(BarW, BarH));
}

void FParticleCurveEditorPanel::DrawTrackList()
{
	const float Scale = UIScale();
	TArray<FParticleSystemEditor::FCurveTrack>& Tracks = Editor.GetCurveTracks();
	if (Tracks.IsEmpty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(28, 28, 30, 255));
		ImGui::TextWrapped("Set a distribution's Type to Curve in Details, then press the ~ button on the module.");
		ImGui::PopStyleColor();
		return;
	}

	ImDrawList* DL = ImGui::GetWindowDrawList();
	const float RowW = ImGui::GetContentRegionAvail().x;
	const float RowH = 44.0f * Scale;
	const float Box = 12.0f * Scale;
	int32 RemoveIndex = -1;

	for (int32 t = 0; t < static_cast<int32>(Tracks.Num()); ++t)
	{
		FParticleSystemEditor::FCurveTrack& Track = Tracks[t];
		ImGui::PushID(t);

		const ImVec2 Pos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##Track", ImVec2(RowW, RowH), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		const bool bHovered = ImGui::IsItemHovered();
		const bool bUsable = Track.IsCurveType();       // 디테일에서 Type을 다른 걸로 바꿨으면 흐리게

		// 채널 상자 위치 (이름 아래 줄)
		auto BoxMin = [&](int32 c) { return ImVec2(Pos.x + 10.0f * Scale + c * (Box + 4.0f * Scale), Pos.y + RowH - Box - 7.0f * Scale); };

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			const ImVec2 M = ImGui::GetMousePos();
			int32 ClickedChannel = -1;
			for (int32 c = 0; c < Track.NumChannels; ++c)
			{
				const ImVec2 B = BoxMin(c);
				if (M.x >= B.x && M.x <= B.x + Box && M.y >= B.y && M.y <= B.y + Box) ClickedChannel = c;
			}

			if (ClickedChannel >= 0)               // 상자: 보이기 / 숨기기
			{
				Track.bChannelVisible[ClickedChannel] = !Track.bChannelVisible[ClickedChannel];
				if (SelectedTrack == t && SelectedChannel == ClickedChannel) SelectedKey = -1;
			}
			else                                   // 줄: 트랙 선택 (키 추가 대상)
			{
				SelectedTrack = t;
				SelectedKey = -1;
				if (SelectedChannel >= Track.NumChannels || !Track.bChannelVisible[SelectedChannel]) SelectedChannel = 0;
			}
		}
		if (ImGui::BeginPopupContextItem("##TrackMenu"))
		{
			if (ImGui::MenuItem("Remove from Curve Editor")) RemoveIndex = t;
			ImGui::EndPopup();
		}
		const bool bSelected = SelectedTrack == t;

		// 배경 + 왼쪽 띠 (선택이면 주황)
		const ImU32 Bg = bSelected ? IM_COL32(74, 74, 80, 255) : bHovered ? IM_COL32(62, 62, 67, 255) : IM_COL32(50, 50, 54, 255);
		DL->AddRectFilled(Pos, ImVec2(Pos.x + RowW, Pos.y + RowH), Bg, 3.0f * Scale);
		DL->AddRectFilled(Pos, ImVec2(Pos.x + 4.0f * Scale, Pos.y + RowH), bSelected ? ColorSelected : IM_COL32(95, 95, 100, 255),
			3.0f * Scale, ImDrawFlags_RoundCornersLeft);

		// 이름 (왼쪽 위) + 모듈 이름 (오른쪽 아래)
		DL->AddText(ImVec2(Pos.x + 10.0f * Scale, Pos.y + 5.0f * Scale),
			bUsable ? IM_COL32(235, 236, 240, 255) : IM_COL32(130, 130, 136, 255), Track.Label.c_str());
		const FString Sub = bUsable ? ShortModuleName(Track.Module) : FString("(not Curve)");
		const ImVec2 SubSize = ImGui::CalcTextSize(Sub.c_str());
		DL->AddText(ImVec2(Pos.x + RowW - SubSize.x - 6.0f * Scale, Pos.y + RowH - SubSize.y - 6.0f * Scale),
			IM_COL32(150, 150, 158, 255), Sub.c_str());

		// 채널 상자: 채워져 있으면 보임. 선택 채널은 흰 테두리
		for (int32 c = 0; c < Track.NumChannels; ++c)
		{
			const ImVec2 B = BoxMin(c);
			const ImVec2 E(B.x + Box, B.y + Box);
			const ImU32 Color = CurveColor(Track.NumChannels, c);
			if (Track.bChannelVisible[c]) DL->AddRectFilled(B, E, Color, 2.0f * Scale);
			else DL->AddRect(B, E, Color, 2.0f * Scale, 0, 1.5f);
			if (bSelected && SelectedChannel == c)
				DL->AddRect(ImVec2(B.x - 2.0f, B.y - 2.0f), ImVec2(E.x + 2.0f, E.y + 2.0f), IM_COL32(255, 255, 255, 220), 2.0f * Scale);
		}

		ImGui::Dummy(ImVec2(0.0f, 1.0f * Scale));
		ImGui::PopID();
	}

	if (RemoveIndex >= 0)                 // 그리는 반복이 끝난 뒤 지운다
	{
		Tracks.RemoveAt(RemoveIndex);
		SelectedTrack = -1;
		SelectedKey = -1;
	}
}

void FParticleCurveEditorPanel::FitHorizontal()
{
	float Lo = 0.0f, Hi = 1.0f;
	for (const FParticleSystemEditor::FCurveTrack& Track : Editor.GetCurveTracks())
		for (int32 c = 0; c < Track.NumChannels; ++c)
		{
			const FParticleCurve* Curve = Track.GetChannel(c);
			if (!Track.bChannelVisible[c] || Curve->Keys.IsEmpty()) continue;
			Lo = std::min(Lo, Curve->Keys[0].Time);
			Hi = std::max(Hi, Curve->Keys[Curve->Keys.Num() - 1].Time);
		}
	const float Pad = (Hi - Lo) * 0.05f;
	ViewMinT = Lo - Pad;
	ViewMaxT = Hi + Pad;
}

void FParticleCurveEditorPanel::FitVertical()
{
	bool bAny = false;
	float Lo = 0.0f, Hi = 1.0f;
	for (const FParticleSystemEditor::FCurveTrack& Track : Editor.GetCurveTracks())
		for (int32 c = 0; c < Track.NumChannels; ++c)
		{
			const FParticleCurve* Curve = Track.GetChannel(c);
			if (!Track.bChannelVisible[c] || Curve->Keys.IsEmpty()) continue;
			float CLo, CHi;
			Curve->GetValueRange(CLo, CHi);
			Lo = bAny ? std::min(Lo, CLo) : CLo;
			Hi = bAny ? std::max(Hi, CHi) : CHi;
			bAny = true;
		}
	if (Hi - Lo < 1e-3f) { Lo -= 0.5f; Hi += 0.5f; }     // 평평한 커브도 보이게
	const float Pad = (Hi - Lo) * 0.1f;
	ViewMinV = Lo - Pad;
	ViewMaxV = Hi + Pad;
}

void FParticleCurveEditorPanel::DrawGraph()
{
	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const ImVec2 Size = ImGui::GetContentRegionAvail();
	if (Size.x < 10.0f || Size.y < 10.0f) return;

	ImGui::InvisibleButton("##Graph", Size,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	const bool bHovered = ImGui::IsItemHovered();
	const bool bActive = ImGui::IsItemActive();
	ImDrawList* DL = ImGui::GetWindowDrawList();
	DL->PushClipRect(Origin, ImVec2(Origin.x + Size.x, Origin.y + Size.y), true);
	DL->AddRectFilled(Origin, ImVec2(Origin.x + Size.x, Origin.y + Size.y), IM_COL32(56, 57, 60, 255));

	// ── 좌표 변환: (시간, 값) ↔ 화면 ──
	auto ToScreen = [&](float T, float V)
		{
			return ImVec2(Origin.x + (T - ViewMinT) / (ViewMaxT - ViewMinT) * Size.x,
				Origin.y + (1.0f - (V - ViewMinV) / (ViewMaxV - ViewMinV)) * Size.y);
		};
	auto ToCurve = [&](ImVec2 P, float& T, float& V)
		{
			T = ViewMinT + (P.x - Origin.x) / Size.x * (ViewMaxT - ViewMinT);
			V = ViewMinV + (1.0f - (P.y - Origin.y) / Size.y) * (ViewMaxV - ViewMinV);
		};

	// ── 격자와 눈금 ──
	const float StepT = NiceStep(ViewMaxT - ViewMinT);
	const float StepV = NiceStep(ViewMaxV - ViewMinV);
	for (float T = std::ceil(ViewMinT / StepT) * StepT; T <= ViewMaxT; T += StepT)
	{
		const float X = ToScreen(T, 0).x;
		// 수명 시작(0)과 끝(1)은 조금 더 밝게
		const bool bLifeEdge = std::fabs(T) < StepT * 0.01f || std::fabs(T - 1.0f) < StepT * 0.01f;
		DL->AddLine(ImVec2(X, Origin.y), ImVec2(X, Origin.y + Size.y), bLifeEdge ? IM_COL32(120, 122, 130, 255) : IM_COL32(76, 77, 82, 255));
		char Buf[16]; snprintf(Buf, sizeof(Buf), "%.2f", T);
		DL->AddText(ImVec2(X + 3, Origin.y + Size.y - ImGui::GetFontSize() - 3), IM_COL32(175, 178, 186, 255), Buf);
	}
	for (float V = std::ceil(ViewMinV / StepV) * StepV; V <= ViewMaxV; V += StepV)
	{
		const float Y = ToScreen(0, V).y;
		const bool bZero = std::fabs(V) < StepV * 0.01f;
		DL->AddLine(ImVec2(Origin.x, Y), ImVec2(Origin.x + Size.x, Y), bZero ? IM_COL32(150, 152, 160, 255) : IM_COL32(76, 77, 82, 255));
		char Buf[16]; snprintf(Buf, sizeof(Buf), "%.2f", V);
		DL->AddText(ImVec2(Origin.x + 4, Y + 1), IM_COL32(175, 178, 186, 255), Buf);
	}

	// ── 곡선과 키 그리기 + 마우스 아래 키 찾기 ──
	const ImVec2 Mouse = ImGui::GetMousePos();
	int32 HoverTrack = -1, HoverChannel = -1, HoverKey = -1;
	TArray<FParticleSystemEditor::FCurveTrack>& Tracks = Editor.GetCurveTracks();
	for (int32 t = 0; t < static_cast<int32>(Tracks.Num()); ++t)
	{
		const FParticleSystemEditor::FCurveTrack& Track = Tracks[t];
		for (int32 c = 0; c < Track.NumChannels; ++c)
		{
			if (!Track.bChannelVisible[c]) continue;
			const FParticleCurve* Curve = Track.GetChannel(c);
			const ImU32 Color = CurveColor(Track.NumChannels, c);
			const bool bSelectedCurve = (t == SelectedTrack && c == SelectedChannel);

			constexpr int32 Samples = 128;           // 보이는 범위를 128조각으로 샘플링
			ImVec2 Prev = ToScreen(ViewMinT, Curve->Eval(ViewMinT, 0.0f));
			for (int32 i = 1; i <= Samples; ++i)
			{
				const float T = ViewMinT + (ViewMaxT - ViewMinT) * i / Samples;
				const ImVec2 P = ToScreen(T, Curve->Eval(T, 0.0f));
				DL->AddLine(Prev, P, Color, bSelectedCurve ? 2.0f : 1.2f);
				Prev = P;
			}

			for (int32 k = 0; k < static_cast<int32>(Curve->Keys.Num()); ++k)
			{
				const ImVec2 P = ToScreen(Curve->Keys[k].Time, Curve->Keys[k].Value);
				const bool bSel = bSelectedCurve && k == SelectedKey;
				const bool bKeyHovered = bHovered && std::fabs(Mouse.x - P.x) <= 6.0f && std::fabs(Mouse.y - P.y) <= 6.0f;
				if (bKeyHovered) { HoverTrack = t; HoverChannel = c; HoverKey = k; }

				// 키: 검은 테두리 사각형. 선택은 흰색, 마우스를 올리면 조금 크게
				const float H = (bSel || bKeyHovered) ? 5.0f : 4.0f;
				DL->AddRectFilled(ImVec2(P.x - H - 1, P.y - H - 1), ImVec2(P.x + H + 1, P.y + H + 1), IM_COL32(15, 15, 18, 255));
				DL->AddRectFilled(ImVec2(P.x - H, P.y - H), ImVec2(P.x + H, P.y + H), bSel ? IM_COL32(255, 255, 255, 255) : Color);
			}
		}
	}

	const ImGuiIO& IO = ImGui::GetIO();

	// ── 왼쪽 클릭: 키 선택 / Ctrl+클릭 · 더블클릭: 키 추가 ──
	const bool bAddKey = bHovered && HoverKey < 0 &&
		((ImGui::IsMouseClicked(ImGuiMouseButton_Left) && IO.KeyCtrl) || ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left));
	if (bAddKey)
	{
		float T, V;
		ToCurve(Mouse, T, V);

		// 넣을 커브: 선택한 커브가 보이면 그것, 아니면 마우스 시간에서 세로로 가장 가까운 보이는 커브
		const bool bSelectedVisible = GetSelectedCurve() && Tracks[SelectedTrack].bChannelVisible[SelectedChannel];
		if (!bSelectedVisible)
		{
			float Best = FLT_MAX;
			for (int32 t = 0; t < static_cast<int32>(Tracks.Num()); ++t)
				for (int32 c = 0; c < Tracks[t].NumChannels; ++c)
				{
					if (!Tracks[t].bChannelVisible[c]) continue;
					const float D = std::fabs(Tracks[t].GetChannel(c)->Eval(T, 0.0f) - V);
					if (D < Best) { Best = D; SelectedTrack = t; SelectedChannel = c; }
				}
		}

		if (FParticleCurve* Curve = GetSelectedCurve())
		{
			SelectedKey = Curve->AddKey(T, V);      // 클릭한 자리에 찍는다
			bDraggingKey = true;                    // 누른 채로 끌면 바로 옮겨진다
			Editor.NotifyPropertyChanged();
		}
	}
	else if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (HoverKey >= 0)
		{
			SelectedTrack = HoverTrack; SelectedChannel = HoverChannel; SelectedKey = HoverKey;
			bDraggingKey = true;
		}
		else
		{
			SelectedKey = -1;                        // 빈 곳: 선택 해제 + 툴바 Pan / Zoom 드래그 시작
			bDraggingView = true;
			ViewDragAnchorX = Mouse.x;
			ViewDragAnchorY = Mouse.y;
		}
	}

	// ── 빈 곳 왼쪽 드래그: Pan 모드는 이동, Zoom 모드는 누른 점을 고정점으로 확대 ──
	if (bDraggingView)
	{
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || bDraggingKey)
		{
			bDraggingView = false;
		}
		else if (IO.MouseDelta.x != 0.0f || IO.MouseDelta.y != 0.0f)
		{
			if (!bZoomMode)
			{
				const float DT = -IO.MouseDelta.x / Size.x * (ViewMaxT - ViewMinT);
				const float DV = IO.MouseDelta.y / Size.y * (ViewMaxV - ViewMinV);
				ViewMinT += DT; ViewMaxT += DT;
				ViewMinV += DV; ViewMaxV += DV;
			}
			else
			{
				float T0, V0;
				ToCurve(ImVec2(ViewDragAnchorX, ViewDragAnchorY), T0, V0);
				const float ST = std::exp(-IO.MouseDelta.x * 0.01f);   // 오른쪽으로 끌면 가로 확대
				const float SV = std::exp(IO.MouseDelta.y * 0.01f);    // 위로 끌면 세로 확대
				ViewMinT = T0 + (ViewMinT - T0) * ST; ViewMaxT = T0 + (ViewMaxT - T0) * ST;
				ViewMinV = V0 + (ViewMinV - V0) * SV; ViewMaxV = V0 + (ViewMaxV - V0) * SV;
			}
		}
	}

	// ── 드래그: 선택한 키 이동 (이웃 키 사이로 시간을 제한해 순서를 유지) ──
	if (bDraggingKey)
	{
		FParticleCurve* Curve = GetSelectedCurve();
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !Curve || SelectedKey < 0 || SelectedKey >= static_cast<int32>(Curve->Keys.Num()))
		{
			bDraggingKey = false;
		}
		else if (IO.MouseDelta.x != 0.0f || IO.MouseDelta.y != 0.0f)
		{
			float T, V;
			ToCurve(Mouse, T, V);
			const int32 Num = static_cast<int32>(Curve->Keys.Num());
			const float MinT = SelectedKey > 0 ? Curve->Keys[SelectedKey - 1].Time + 1e-3f : -1e9f;
			const float MaxT = SelectedKey < Num - 1 ? Curve->Keys[SelectedKey + 1].Time - 1e-3f : 1e9f;
			Curve->Keys[SelectedKey].Time = std::clamp(T, MinT, MaxT);
			Curve->Keys[SelectedKey].Value = V;
			Curve->AutoSetTangents();
			Editor.NotifyPropertyChanged();     // 손을 뗄 때까지 재시작은 미뤄진다 (InvisibleButton이 active)
		}

		// 끄는 동안 키 옆에 값 표시
		if (bDraggingKey && Curve)
		{
			const FParticleCurveKey& K = Curve->Keys[SelectedKey];
			char Buf[48]; snprintf(Buf, sizeof(Buf), "%.3f, %.3f", K.Time, K.Value);
			const ImVec2 P = ToScreen(K.Time, K.Value);
			const ImVec2 TS = ImGui::CalcTextSize(Buf);
			const ImVec2 A(P.x + 10, P.y - TS.y - 10);
			DL->AddRectFilled(ImVec2(A.x - 4, A.y - 2), ImVec2(A.x + TS.x + 4, A.y + TS.y + 2), IM_COL32(20, 20, 24, 220), 3.0f);
			DL->AddText(A, IM_COL32(240, 240, 245, 255), Buf);
		}
	}

	// ── Delete: 선택한 키 삭제 ──
	if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		if (FParticleCurve* Curve = GetSelectedCurve(); Curve && SelectedKey >= 0)
		{
			Curve->RemoveKeyAt(SelectedKey);
			SelectedKey = -1;
			Editor.NotifyPropertyChanged();
		}
	}

	// ── 오른쪽 · 가운데 드래그: 이동 ──
	if (bActive && (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
	{
		const float DT = -IO.MouseDelta.x / Size.x * (ViewMaxT - ViewMinT);
		const float DV = IO.MouseDelta.y / Size.y * (ViewMaxV - ViewMinV);
		ViewMinT += DT; ViewMaxT += DT;
		ViewMinV += DV; ViewMaxV += DV;
	}

	// ── 휠: 마우스 위치를 고정점으로 확대 (Ctrl이면 세로) ──
	if (bHovered && IO.MouseWheel != 0.0f)
	{
		const float Scale = IO.MouseWheel > 0.0f ? 0.85f : 1.0f / 0.85f;
		float T, V;
		ToCurve(Mouse, T, V);
		if (IO.KeyCtrl)
		{
			ViewMinV = V + (ViewMinV - V) * Scale;
			ViewMaxV = V + (ViewMaxV - V) * Scale;
		}
		else
		{
			ViewMinT = T + (ViewMinT - T) * Scale;
			ViewMaxT = T + (ViewMaxT - T) * Scale;
		}
	}

	DL->PopClipRect();
}