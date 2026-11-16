#include "EnginePCH.h"
#include "Editor/EditorUI/EditorIcons.h"

#include <imgui_internal.h>
#include <cctype>
#include <cmath>

namespace
{
	ImU32 ApplyStyleAlpha(ImU32 Color)
	{
		ImVec4 C = ImGui::ColorConvertU32ToFloat4(Color);
		C.w *= ImGui::GetStyle().Alpha;   // BeginDisabled일 때 흐리게
		return ImGui::ColorConvertFloat4ToU32(C);
	}

	ImU32 Shade(ImU32 Color, float Factor)
	{
		ImVec4 C = ImGui::ColorConvertU32ToFloat4(Color);
		C.x *= Factor; C.y *= Factor; C.z *= Factor;
		return ImGui::ColorConvertFloat4ToU32(C);
	}

	// 아이콘 좌표계: 중심 (0,0), 한 변 1 ([-0.5, 0.5]).
	struct FPen
	{
		ImDrawList* DL;
		ImVec2 Center;
		float S;

		ImVec2 P(float X, float Y) const { return ImVec2(Center.x + X * S, Center.y + Y * S); }
		void Rect(float X0, float Y0, float X1, float Y1, ImU32 Col, float Round = 0.0f) const
		{
			DL->AddRectFilled(P(X0, Y0), P(X1, Y1), Col, Round * S);
		}
		void Tri(float X0, float Y0, float X1, float Y1, float X2, float Y2, ImU32 Col) const
		{
			DL->AddTriangleFilled(P(X0, Y0), P(X1, Y1), P(X2, Y2), Col);
		}
		void Quad(const ImVec2& A, const ImVec2& B, const ImVec2& C, const ImVec2& D, ImU32 Col) const
		{
			DL->AddQuadFilled(A, B, C, D, Col);
		}
		void Circle(float X, float Y, float R, ImU32 Col) const { DL->AddCircleFilled(P(X, Y), R * S, Col); }
		void Ring(float X, float Y, float R, ImU32 Col, float Thick) const { DL->AddCircle(P(X, Y), R * S, Col, 0, Thick * S); }
		void Line(float X0, float Y0, float X1, float Y1, ImU32 Col, float Thick) const
		{
			DL->AddLine(P(X0, Y0), P(X1, Y1), Col, ImMax(1.0f, Thick * S));
		}
		void Polyline(std::initializer_list<ImVec2> Points, ImU32 Col, float Thick) const
		{
			ImVec2 Buffer[8];
			int Count = 0;
			for (const ImVec2& Pt : Points)
				Buffer[Count++] = P(Pt.x, Pt.y);
			DL->AddPolyline(Buffer, Count, Col, ImDrawFlags_None, ImMax(1.5f, Thick * S));
		}
	};

	// 면마다 밝기가 다른 등각 큐브. (X, Y)가 중심, R은 꼭짓점까지 거리.
	void DrawCube(const FPen& Pen, float X, float Y, float R, ImU32 Col)
	{
		auto At = [&](float U, float V) { return Pen.P(X + U * R, Y + V * R); };
		const ImVec2 Top = At(0.0f, -1.0f), UR = At(0.866f, -0.5f), LR = At(0.866f, 0.5f);
		const ImVec2 Bottom = At(0.0f, 1.0f), LL = At(-0.866f, 0.5f), UL = At(-0.866f, -0.5f), Mid = At(0.0f, 0.0f);
		Pen.Quad(Top, UR, Mid, UL, Col);
		Pen.Quad(UL, Mid, Bottom, LL, Shade(Col, 0.78f));
		Pen.Quad(Mid, UR, LR, Bottom, Shade(Col, 0.58f));
	}

	// 원호 + 화살촉. 원호는 AMin에서 AMax로(화면 좌표, 시계 방향) 그린다.
	// bHeadAtMin이면 AMin 끝에, 아니면 AMax 끝에 원호 바깥쪽을 향한 화살촉을 단다.
	void DrawArcArrow(const FPen& Pen, float Radius, float AMin, float AMax, bool bHeadAtMin, ImU32 Col, float Thick)
	{
		Pen.DL->PathArcTo(Pen.Center, Radius * Pen.S, AMin, AMax, 24);
		Pen.DL->PathStroke(Col, ImDrawFlags_None, ImMax(1.5f, Thick * Pen.S));

		const float A = bHeadAtMin ? AMin : AMax;
		const ImVec2 Normal(std::cos(A), std::sin(A));
		// 각도가 줄어드는 쪽(Min 끝) 또는 늘어나는 쪽(Max 끝)의 접선
		const ImVec2 Tangent = bHeadAtMin ? ImVec2(Normal.y, -Normal.x) : ImVec2(-Normal.y, Normal.x);
		const ImVec2 Base(Pen.Center.x + Normal.x * Radius * Pen.S, Pen.Center.y + Normal.y * Radius * Pen.S);
		const float Head = 0.17f * Pen.S;
		Pen.DL->AddTriangleFilled(
			ImVec2(Base.x + Tangent.x * Head, Base.y + Tangent.y * Head),
			ImVec2(Base.x + Normal.x * Head * 0.8f, Base.y + Normal.y * Head * 0.8f),
			ImVec2(Base.x - Normal.x * Head * 0.8f, Base.y - Normal.y * Head * 0.8f), Col);
	}

	void DrawFolder(const FPen& Pen, ImU32 Col, float Right)
	{
		Pen.Rect(-0.46f, -0.38f, -0.08f, -0.22f, Col, 0.04f);   // 탭
		Pen.Rect(-0.46f, -0.28f, Right, 0.34f, Col, 0.05f);     // 몸통
	}
}

namespace EditorIcons
{
	float GetScale()
	{
		return ImGui::GetFontSize() / 13.0f;
	}

	void Draw(ImDrawList* DrawList, EEditorIcon Icon, const ImVec2& Center, float Size, ImU32 Color, ImU32 Bg)
	{
		const FPen Pen{ DrawList, Center, Size };
		const ImU32 C = Color;

		switch (Icon)
		{
		case EEditorIcon::Save:
		{
			// 디스켓: 오른쪽 위가 깎인 몸통 + 셔터 + 라벨
			const ImVec2 Body[] = { Pen.P(-0.42f, -0.42f), Pen.P(0.26f, -0.42f), Pen.P(0.42f, -0.26f),
				Pen.P(0.42f, 0.42f), Pen.P(-0.42f, 0.42f) };
			DrawList->AddConvexPolyFilled(Body, IM_ARRAYSIZE(Body), C);
			Pen.Rect(-0.24f, -0.42f, 0.16f, -0.12f, Bg);
			Pen.Rect(0.02f, -0.36f, 0.11f, -0.18f, C);
			Pen.Rect(-0.27f, 0.06f, 0.27f, 0.42f, Bg, 0.03f);
			break;
		}
		case EEditorIcon::BrowseContent:
		{
			// 폴더 + 돋보기
			DrawFolder(Pen, C, 0.30f);
			Pen.Circle(0.16f, 0.12f, 0.27f, Bg);
			Pen.Line(0.28f, 0.24f, 0.46f, 0.44f, Bg, 0.22f);
			Pen.Ring(0.16f, 0.12f, 0.15f, C, 0.08f);
			Pen.Line(0.27f, 0.23f, 0.43f, 0.41f, C, 0.10f);
			break;
		}
		case EEditorIcon::Folder:
			DrawFolder(Pen, C, 0.46f);
			break;
		case EEditorIcon::AddActor:
		{
			// 면마다 밝기가 다른 큐브 + 초록 +
			const float R = 0.40f;
			const ImVec2 O = Pen.P(-0.07f, -0.05f);
			auto At = [&](float X, float Y) { return ImVec2(O.x + X * R * Size, O.y + Y * R * Size); };
			const ImVec2 Top = At(0.0f, -1.0f), UR = At(0.866f, -0.5f), LR = At(0.866f, 0.5f);
			const ImVec2 Bottom = At(0.0f, 1.0f), LL = At(-0.866f, 0.5f), UL = At(-0.866f, -0.5f), Mid = At(0.0f, 0.0f);
			Pen.Quad(Top, UR, Mid, UL, C);
			Pen.Quad(UL, Mid, Bottom, LL, Shade(C, 0.78f));
			Pen.Quad(Mid, UR, LR, Bottom, Shade(C, 0.58f));

			const float X = 0.27f, Y = 0.27f, Half = 0.21f, Thick = 0.065f, Gap = 0.06f;
			Pen.Rect(X - Half - Gap, Y - Thick - Gap, X + Half + Gap, Y + Thick + Gap, Bg);
			Pen.Rect(X - Thick - Gap, Y - Half - Gap, X + Thick + Gap, Y + Half + Gap, Bg);
			const ImU32 Green = ApplyStyleAlpha(PlayGreen);
			Pen.Rect(X - Half, Y - Thick, X + Half, Y + Thick, Green);
			Pen.Rect(X - Thick, Y - Half, X + Thick, Y + Half, Green);
			break;
		}
		case EEditorIcon::Cinematics:
		{
			// 클래퍼보드: 줄무늬 막대 + 경첩 + 몸통
			Pen.Rect(-0.44f, -0.40f, 0.44f, -0.18f, C, 0.03f);
			for (int32 I = 0; I < 3; ++I)
			{
				const float X = -0.28f + I * 0.28f;
				Pen.Quad(Pen.P(X, -0.40f), Pen.P(X + 0.10f, -0.40f), Pen.P(X + 0.02f, -0.18f), Pen.P(X - 0.08f, -0.18f), Bg);
			}
			Pen.Rect(-0.44f, -0.18f, -0.34f, -0.06f, C);
			Pen.Rect(-0.44f, -0.06f, 0.44f, 0.40f, C, 0.04f);
			break;
		}
		case EEditorIcon::Play:
			Pen.Tri(-0.32f, -0.42f, -0.32f, 0.42f, 0.42f, 0.0f, C);
			break;
		case EEditorIcon::Pause:
			Pen.Rect(-0.30f, -0.38f, -0.08f, 0.38f, C, 0.02f);
			Pen.Rect(0.08f, -0.38f, 0.30f, 0.38f, C, 0.02f);
			break;
		case EEditorIcon::Step:
			Pen.Rect(-0.40f, -0.36f, -0.26f, 0.36f, C);
			Pen.Tri(-0.16f, -0.38f, -0.16f, 0.38f, 0.40f, 0.0f, C);
			break;
		case EEditorIcon::Stop:
			Pen.Rect(-0.34f, -0.34f, 0.34f, 0.34f, C, 0.03f);
			break;
		case EEditorIcon::MoreVertical:
			Pen.Circle(0.0f, -0.30f, 0.085f, C);
			Pen.Circle(0.0f, 0.0f, 0.085f, C);
			Pen.Circle(0.0f, 0.30f, 0.085f, C);
			break;
		case EEditorIcon::ChevronDown:
			Pen.Polyline({ ImVec2(-0.38f, -0.18f), ImVec2(0.0f, 0.20f), ImVec2(0.38f, -0.18f) }, C, 0.16f);
			break;
		case EEditorIcon::Settings:
		{
			// 톱니 8개 + 몸통 + 가운데 구멍
			for (int32 I = 0; I < 8; ++I)
			{
				const float A = I * IM_PI * 0.25f;
				const ImVec2 D(std::cos(A), std::sin(A));
				const ImVec2 N(-D.y, D.x);
				const float W = 0.085f, R0 = 0.22f, R1 = 0.46f;
				Pen.Quad(Pen.P(D.x * R0 - N.x * W, D.y * R0 - N.y * W), Pen.P(D.x * R1 - N.x * W, D.y * R1 - N.y * W),
					Pen.P(D.x * R1 + N.x * W, D.y * R1 + N.y * W), Pen.P(D.x * R0 + N.x * W, D.y * R0 + N.y * W), C);
			}
			Pen.Circle(0.0f, 0.0f, 0.33f, C);
			Pen.Circle(0.0f, 0.0f, 0.13f, Bg);
			break;
		}
		case EEditorIcon::NewWindow:
			DrawList->AddRect(Pen.P(-0.44f, -0.36f), Pen.P(0.44f, 0.38f), C, 0.04f * Size, 0, ImMax(1.0f, 0.08f * Size));
			Pen.Rect(-0.44f, -0.36f, 0.44f, -0.16f, C);
			Pen.Tri(-0.08f, -0.04f, -0.08f, 0.26f, 0.18f, 0.11f, C);
			break;
		case EEditorIcon::Check:
			Pen.Polyline({ ImVec2(-0.34f, 0.0f), ImVec2(-0.10f, 0.26f), ImVec2(0.36f, -0.26f) }, C, 0.13f);
			break;
		case EEditorIcon::SelectMode:
		{
			// 큐브 + 오른쪽 아래 마우스 커서 (바탕색으로 테두리를 둘러 큐브와 떨어뜨린다)
			DrawCube(Pen, -0.12f, -0.10f, 0.34f, C);
			const ImVec2 Tip = Pen.P(0.04f, -0.04f), Left = Pen.P(0.04f, 0.42f), Right = Pen.P(0.38f, 0.26f);
			DrawList->AddTriangle(Tip, Left, Right, Bg, 0.12f * Size);
			DrawList->AddTriangleFilled(Tip, Left, Right, C);
			Pen.Line(0.17f, 0.30f, 0.28f, 0.48f, Bg, 0.20f);
			Pen.Line(0.17f, 0.30f, 0.27f, 0.46f, C, 0.09f);
			break;
		}
		case EEditorIcon::Eye:
		case EEditorIcon::EyeClosed:
		{
			constexpr int32 Steps = 12;
			ImVec2 Points[Steps * 2 + 2];
			int32 Count = 0;
			for (int32 I = 0; I <= Steps; ++I)
			{
				const float T = static_cast<float>(I) / Steps;
				Points[Count++] = Pen.P(-0.46f + 0.92f * T, -0.28f * std::sin(IM_PI * T));
			}
			if (Icon == EEditorIcon::Eye)
			{
				for (int32 I = Steps - 1; I >= 1; --I)
				{
					const float T = static_cast<float>(I) / Steps;
					Points[Count++] = Pen.P(-0.46f + 0.92f * T, 0.28f * std::sin(IM_PI * T));
				}
				DrawList->AddConvexPolyFilled(Points, Count, C);
				Pen.Circle(0.0f, 0.0f, 0.17f, Bg);
				Pen.Circle(0.0f, 0.0f, 0.09f, C);
			}
			else
			{
				// 감은 눈: 아래로 휜 선 + 속눈썹
				for (int32 I = 0; I <= Steps; ++I)
					Points[I].y = Pen.P(0.0f, 0.18f * std::sin(IM_PI * I / Steps) - 0.05f).y;
				DrawList->AddPolyline(Points, Steps + 1, C, ImDrawFlags_None, ImMax(1.5f, 0.09f * Size));
				Pen.Line(-0.24f, 0.08f, -0.32f, 0.26f, C, 0.08f);
				Pen.Line(0.0f, 0.13f, 0.0f, 0.33f, C, 0.08f);
				Pen.Line(0.24f, 0.08f, 0.32f, 0.26f, C, 0.08f);
			}
			break;
		}
		case EEditorIcon::Translate:
		{
			Pen.Line(-0.34f, 0.0f, 0.34f, 0.0f, C, 0.08f);
			Pen.Line(0.0f, -0.34f, 0.0f, 0.34f, C, 0.08f);
			Pen.Tri(-0.48f, 0.0f, -0.30f, -0.13f, -0.30f, 0.13f, C);
			Pen.Tri(0.48f, 0.0f, 0.30f, -0.13f, 0.30f, 0.13f, C);
			Pen.Tri(0.0f, -0.48f, -0.13f, -0.30f, 0.13f, -0.30f, C);
			Pen.Tri(0.0f, 0.48f, -0.13f, 0.30f, 0.13f, 0.30f, C);
			break;
		}
		case EEditorIcon::Rotate:
			DrawArcArrow(Pen, 0.33f, -0.40f * IM_PI, 0.85f * IM_PI, true, C, 0.10f);
			break;
		case EEditorIcon::Reset:
			DrawArcArrow(Pen, 0.30f, -0.15f * IM_PI, 1.10f * IM_PI, false, C, 0.10f);
			break;
		case EEditorIcon::Scale:
		{
			DrawList->AddRect(Pen.P(-0.42f, -0.10f), Pen.P(0.10f, 0.42f), C, 0.0f, 0, ImMax(1.0f, 0.08f * Size));
			Pen.Rect(0.14f, -0.42f, 0.42f, -0.14f, C, 0.02f);
			Pen.Line(-0.10f, 0.10f, 0.20f, -0.20f, C, 0.08f);
			break;
		}
		case EEditorIcon::World:
		{
			const float Thick = ImMax(1.0f, 0.07f * Size);
			Pen.Ring(0.0f, 0.0f, 0.42f, C, 0.07f);
			DrawList->AddEllipse(Pen.Center, ImVec2(0.18f * Size, 0.42f * Size), C, 0.0f, 0, Thick);
			Pen.Line(-0.42f, 0.0f, 0.42f, 0.0f, C, 0.07f);
			Pen.Line(-0.36f, -0.21f, 0.36f, -0.21f, C, 0.06f);
			Pen.Line(-0.36f, 0.21f, 0.36f, 0.21f, C, 0.06f);
			break;
		}
		case EEditorIcon::Local:
			DrawCube(Pen, 0.0f, 0.0f, 0.44f, C);
			break;
		case EEditorIcon::Search:
			Pen.Ring(-0.08f, -0.08f, 0.26f, C, 0.10f);
			Pen.Line(0.12f, 0.12f, 0.40f, 0.40f, C, 0.13f);
			break;
		case EEditorIcon::Camera:
			Pen.Rect(-0.46f, -0.26f, 0.16f, 0.28f, C, 0.06f);
			Pen.Quad(Pen.P(0.16f, -0.04f), Pen.P(0.46f, -0.26f), Pen.P(0.46f, 0.28f), Pen.P(0.16f, 0.06f), C);
			break;
		case EEditorIcon::CameraSpeed:
			Pen.Line(-0.46f, -0.20f, -0.10f, -0.20f, C, 0.08f);
			Pen.Line(-0.40f, 0.0f, -0.10f, 0.0f, C, 0.08f);
			Pen.Line(-0.46f, 0.20f, -0.10f, 0.20f, C, 0.08f);
			Pen.Tri(-0.02f, -0.34f, -0.02f, 0.34f, 0.46f, 0.0f, C);
			break;
		case EEditorIcon::Lit:
			Pen.Circle(0.0f, 0.0f, 0.42f, Shade(C, 0.45f));
			Pen.Circle(-0.07f, -0.07f, 0.33f, C);
			break;
		case EEditorIcon::LayoutQuad:
			Pen.Rect(-0.42f, -0.36f, -0.03f, -0.03f, C, 0.03f);
			Pen.Rect(0.03f, -0.36f, 0.42f, -0.03f, C, 0.03f);
			Pen.Rect(-0.42f, 0.03f, -0.03f, 0.36f, C, 0.03f);
			Pen.Rect(0.03f, 0.03f, 0.42f, 0.36f, C, 0.03f);
			break;
		case EEditorIcon::LayoutSingle:
			Pen.Rect(-0.42f, -0.36f, 0.42f, 0.36f, C, 0.04f);
			break;
		case EEditorIcon::Select:
		{
			// 화살표 커서 (오목한 모양이라 AddConcavePolyFilled)
			const ImVec2 Arrow[] = { Pen.P(-0.28f, -0.46f), Pen.P(-0.28f, 0.30f), Pen.P(-0.10f, 0.13f),
				Pen.P(0.04f, 0.44f), Pen.P(0.17f, 0.38f), Pen.P(0.03f, 0.08f), Pen.P(0.27f, 0.08f) };
			DrawList->AddConcavePolyFilled(Arrow, IM_ARRAYSIZE(Arrow), C);
			break;
		}
		case EEditorIcon::None:
		default:
			break;
		}
	}

	void MenuNote(const char* Text, float WrapWidth)
	{
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + WrapWidth * GetScale());
		ImGui::TextDisabled("%s", Text);
		ImGui::PopTextWrapPos();
	}

	bool SearchBox(const char* Id, char* Buffer, size_t BufferSize, const char* Hint)
	{
		const float S = GetScale();
		const float IconSize = 12.0f * S;
		const float IconPad = IconSize + 10.0f * S;

		// 왼쪽 여백만 넓혀 돋보기 자리를 만든다.
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(IconPad, ImGui::GetStyle().FramePadding.y + 1.0f * S));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f * S);
		ImGui::SetNextItemWidth(-FLT_MIN);
		const bool bChanged = ImGui::InputTextWithHint(Id, Hint, Buffer, BufferSize);
		ImGui::PopStyleVar(2);

		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		Draw(ImGui::GetWindowDrawList(), EEditorIcon::Search,
			ImVec2(Min.x + 6.0f * S + IconSize * 0.5f, (Min.y + Max.y) * 0.5f), IconSize,
			ImGui::GetColorU32(ImGuiCol_TextDisabled), ImGui::GetColorU32(ImGuiCol_FrameBg));
		return bChanged;
	}

	bool MatchesFilter(const char* Text, const char* Filter)
	{
		if (!Filter || !Filter[0])
			return true;
		if (!Text)
			return false;
		for (const char* Start = Text; *Start; ++Start)
		{
			const char* T = Start;
			const char* F = Filter;
			while (*T && *F && std::tolower(static_cast<unsigned char>(*T)) == std::tolower(static_cast<unsigned char>(*F)))
			{
				++T;
				++F;
			}
			if (!*F)
				return true;
		}
		return false;
	}

	float CalcButtonWidth(bool bHasIcon, const char* Label, bool bDropdown, float Height)
	{
		const float S = GetScale();
		const float H = Height > 0.0f ? Height : 28.0f * S;
		if (!Label && !bDropdown)
			return H;   // 아이콘만 있으면 정사각형

		const float IconSize = ImMin(18.0f * S, H - 8.0f * S);
		float Width = 6.0f * S;
		if (bHasIcon) Width += IconSize;
		if (Label) Width += (bHasIcon ? 6.0f * S : 0.0f) + ImGui::CalcTextSize(Label, nullptr, true).x;
		if (bDropdown) Width += 5.0f * S + 8.0f * S;
		return Width + 6.0f * S;
	}

	bool Button(const char* Id, EEditorIcon Icon, ImU32 IconColor, const char* Label,
		bool bDropdown, bool bSelected, float Height, ImGuiButtonFlags Flags)
	{
		const float S = GetScale();
		const float H = Height > 0.0f ? Height : 28.0f * S;
		const float IconSize = ImMin(18.0f * S, H - 8.0f * S);
		const float PadX = 6.0f * S;
		const float ChevronSize = 8.0f * S;
		const bool bHasIcon = Icon != EEditorIcon::None;
		const float LabelWidth = Label ? ImGui::CalcTextSize(Label, nullptr, true).x : 0.0f;
		const float Width = CalcButtonWidth(bHasIcon, Label, bDropdown, Height);

		const ImVec2 Min = ImGui::GetCursorScreenPos();
		const bool bPressed = ImGui::InvisibleButton(Id, ImVec2(Width, H), Flags);
		const bool bHovered = ImGui::IsItemHovered();
		const bool bHeld = ImGui::IsItemActive();
		const ImVec2 Max(Min.x + Width, Min.y + H);

		ImDrawList* DL = ImGui::GetWindowDrawList();
		ImU32 Bg = ImGui::GetColorU32(ImGuiCol_WindowBg);
		if (bHeld || bHovered || bSelected)
		{
			Bg = ImGui::GetColorU32(bHeld ? ImGuiCol_ButtonActive : bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Header);
			DL->AddRectFilled(Min, Max, Bg, 3.0f * S);
		}

		const float MidY = (Min.y + Max.y) * 0.5f;
		float X = Min.x + PadX;
		if (!Label && !bDropdown)
			X = Min.x + (Width - IconSize) * 0.5f;

		if (bHasIcon)
		{
			Draw(DL, Icon, ImVec2(X + IconSize * 0.5f, MidY), IconSize, ApplyStyleAlpha(IconColor), Bg);
			X += IconSize + 6.0f * S;
		}
		if (Label)
		{
			const float TextH = ImGui::GetFontSize();
			DL->AddText(ImVec2(X, MidY - TextH * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), Label, ImGui::FindRenderedTextEnd(Label));
			X += LabelWidth + 5.0f * S;
		}
		if (bDropdown)
		{
			if (!Label) X -= 2.0f * S;
			Draw(DL, EEditorIcon::ChevronDown, ImVec2(X + ChevronSize * 0.5f, MidY), ChevronSize,
				ImGui::GetColorU32(ImGuiCol_Text, 0.85f), Bg);
		}
		return bPressed;
	}

	void MenuSection(const char* Title)
	{
		const float S = GetScale();
		char Upper[64];
		size_t I = 0;
		for (; Title[I] && I + 1 < sizeof(Upper); ++I)
			Upper[I] = static_cast<char>(std::toupper(static_cast<unsigned char>(Title[I])));
		Upper[I] = '\0';

		ImGui::Dummy(ImVec2(0.0f, 2.0f * S));
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f * S);
		ImGui::TextDisabled("%s", Upper);
	}

	bool MenuRow(const char* Label, EEditorIcon Icon, bool bChecked, bool bEnabled, bool bKeepOpen,
		const char* Shortcut, bool bRadio)
	{
		const float S = GetScale();
		const float H = ImGui::GetFontSize() + 10.0f * S;
		const float CheckX = 12.0f * S, IconX = 34.0f * S, TextX = 54.0f * S;
		const float ShortcutWidth = Shortcut ? ImGui::CalcTextSize(Shortcut).x + 28.0f * S : 0.0f;
		const float RowWidth = TextX + ImGui::CalcTextSize(Label).x + 24.0f * S + ShortcutWidth;

		ImGui::PushID(Label);
		ImGui::BeginDisabled(!bEnabled);
		const ImVec2 Min = ImGui::GetCursorScreenPos();
		const bool bPressed = ImGui::Selectable("##Row", false,
			bKeepOpen ? ImGuiSelectableFlags_NoAutoClosePopups : ImGuiSelectableFlags_None,
			ImVec2(ImMax(RowWidth, ImGui::GetContentRegionAvail().x), H));
		const bool bHovered = ImGui::IsItemHovered();

		ImDrawList* DL = ImGui::GetWindowDrawList();
		const ImU32 TextCol = ImGui::GetColorU32(ImGuiCol_Text);
		const ImU32 Bg = ImGui::GetColorU32(bHovered ? ImGuiCol_HeaderHovered : ImGuiCol_PopupBg);
		const float MidY = Min.y + H * 0.5f;
		const float IconSize = 14.0f * S;
		if (bRadio)
			DL->AddCircleFilled(ImVec2(Min.x + CheckX, MidY), 3.5f * S,
				bChecked ? TextCol : ImGui::GetColorU32(ImGuiCol_WindowBg));
		else if (bChecked)
			Draw(DL, EEditorIcon::Check, ImVec2(Min.x + CheckX, MidY), 12.0f * S, TextCol, Bg);
		if (Icon != EEditorIcon::None)
			Draw(DL, Icon, ImVec2(Min.x + IconX, MidY), IconSize, ImGui::GetColorU32(ImGuiCol_Text, 0.8f), Bg);
		DL->AddText(ImVec2(Min.x + TextX, MidY - ImGui::GetFontSize() * 0.5f), TextCol, Label);
		if (Shortcut)
		{
			const float RowMaxX = ImGui::GetItemRectMax().x;
			DL->AddText(ImVec2(RowMaxX - 10.0f * S - ImGui::CalcTextSize(Shortcut).x, MidY - ImGui::GetFontSize() * 0.5f),
				ImGui::GetColorU32(ImGuiCol_TextDisabled), Shortcut);
		}

		ImGui::EndDisabled();
		ImGui::PopID();
		return bPressed;
	}
}
