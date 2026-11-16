#pragma once

#include "Math/EngineMath.h"
#include "Math/Rect.h"
#include "Math/Vector2.h"

#include <cassert>
#include <cstring>

// 레벨 에디터 뷰포트 배치. UE5 뷰포트 Layout 메뉴의 One/Two/Three/Four Panes와 같다.
// 큰 칸(Left/Right/Top/Bottom)이 있는 배치에서는 큰 칸이 항상 View 0(원근 뷰)이다.
enum class EViewportLayout : uint8
{
	OnePane,
	TwoPanesHoriz,     // 좌우 두 칸
	TwoPanesVert,      // 위아래 두 칸
	ThreePanesLeft,    // 왼쪽 큰 칸 + 오른쪽 두 칸
	ThreePanesRight,   // 왼쪽 두 칸 + 오른쪽 큰 칸
	ThreePanesTop,     // 위쪽 큰 칸 + 아래쪽 두 칸
	ThreePanesBottom,  // 위쪽 두 칸 + 아래쪽 큰 칸
	FourPanes2x2,
	FourPanesLeft,     // 왼쪽 큰 칸 + 오른쪽 세 칸
	FourPanesRight,    // 왼쪽 세 칸 + 오른쪽 큰 칸
	FourPanesTop,      // 위쪽 큰 칸 + 아래쪽 세 칸
	FourPanesBottom,   // 위쪽 세 칸 + 아래쪽 큰 칸

	Count
};

// 설정 파일에 쓰는 이름 (enum 순서)
inline constexpr const char* ViewportLayoutNames[] = {
	"OnePane",
	"TwoPanesHoriz", "TwoPanesVert",
	"ThreePanesLeft", "ThreePanesRight", "ThreePanesTop", "ThreePanesBottom",
	"FourPanes2x2", "FourPanesLeft", "FourPanesRight", "FourPanesTop", "FourPanesBottom",
};
static_assert(sizeof(ViewportLayoutNames) / sizeof(ViewportLayoutNames[0]) == static_cast<size_t>(EViewportLayout::Count));

inline EViewportLayout ViewportLayoutFromName(const char* Name, EViewportLayout Fallback = EViewportLayout::FourPanes2x2)
{
	for (int32 I = 0; I < static_cast<int32>(EViewportLayout::Count); ++I)
		if (std::strcmp(ViewportLayoutNames[I], Name) == 0)
			return static_cast<EViewportLayout>(I);
	return Fallback;
}

inline int32 GetViewportLayoutPaneCount(EViewportLayout Layout)
{
	switch (Layout)
	{
	case EViewportLayout::OnePane:
		return 1;
	case EViewportLayout::TwoPanesHoriz:
	case EViewportLayout::TwoPanesVert:
		return 2;
	case EViewportLayout::ThreePanesLeft:
	case EViewportLayout::ThreePanesRight:
	case EViewportLayout::ThreePanesTop:
	case EViewportLayout::ThreePanesBottom:
		return 3;
	default:
		return 4;
	}
}

// 배치 + 한 칸 최대화 상태. 최대화는 배치를 바꾸지 않고 그 칸만 패널 전체로 키운다 (UE5 Maximize Viewport).
struct FViewportLayoutState
{
	EViewportLayout Layout = EViewportLayout::FourPanes2x2;
	bool bMaximized = false;
	int32 MaximizedViewIndex = 0;

	bool IsShowingMultiplePanes() const { return !bMaximized && GetViewportLayoutPaneCount(Layout) > 1; }
};

enum class EDragAxis { Horizontal, Vertical };

struct FSplitRatio
{
	float Horizontal = 0.5f;          // 세로 분할선의 x 위치 (패널 너비 비율)
	float Vertical = 0.5f;            // 가로 분할선의 y 위치 (패널 높이 비율)
	float Third0 = 1.0f / 3.0f;       // 세 칸 줄의 첫째/둘째 경계 (그 줄 방향 패널 크기 비율)
	float Third1 = 2.0f / 3.0f;
};

// 분할선 하나가 바꾸는 비율 값
enum class ESplitRatioField : uint8 { Horizontal, Vertical, Third0, Third1, Count };

inline float& GetSplitRatioField(FSplitRatio& Ratio, ESplitRatioField Field)
{
	switch (Field)
	{
	case ESplitRatioField::Vertical: return Ratio.Vertical;
	case ESplitRatioField::Third0:   return Ratio.Third0;
	case ESplitRatioField::Third1:   return Ratio.Third1;
	default:                         return Ratio.Horizontal;
	}
}

// 모든 칸이 MinRatio보다 작아지지 않게 자른다. 세 칸 경계는 서로 순서를 지킨다.
inline FSplitRatio ClampSplitRatio(const FSplitRatio& Ratio, float MinRatio)
{
	assert(MinRatio >= 0.0f && MinRatio <= 0.3f);
	FSplitRatio Result;
	Result.Horizontal = FMath::Clamp(Ratio.Horizontal, MinRatio, 1.0f - MinRatio);
	Result.Vertical = FMath::Clamp(Ratio.Vertical, MinRatio, 1.0f - MinRatio);
	Result.Third0 = FMath::Clamp(Ratio.Third0, MinRatio, 1.0f - MinRatio * 2.0f);
	Result.Third1 = FMath::Clamp(Ratio.Third1, Result.Third0 + MinRatio, 1.0f - MinRatio);
	return Result;
}

// 배치만으로 View별 사각형을 구한다 (최대화는 보지 않음). 쓰지 않는 View는 크기 0.
inline void ComputeLayoutRects(EViewportLayout Layout, const FSplitRatio& Ratio, FVector2 Size, FRect OutRects[4])
{
	const float W = Size.X, H = Size.Y;
	const float SX = W * Ratio.Horizontal;
	const float SY = H * Ratio.Vertical;
	for (int32 I = 0; I < 4; ++I)
		OutRects[I] = {};

	// 한 줄을 Third0/Third1 경계로 세 칸으로 나눈다.
	const float Cuts[4] = { 0.0f, Ratio.Third0, Ratio.Third1, 1.0f };
	auto Column3 = [&](float X, float ColumnWidth)
	{
		for (int32 I = 0; I < 3; ++I)
			OutRects[1 + I] = { X, H * Cuts[I], ColumnWidth, H * (Cuts[I + 1] - Cuts[I]) };
	};
	auto Row3 = [&](float Y, float RowHeight)
	{
		for (int32 I = 0; I < 3; ++I)
			OutRects[1 + I] = { W * Cuts[I], Y, W * (Cuts[I + 1] - Cuts[I]), RowHeight };
	};

	switch (Layout)
	{
	case EViewportLayout::OnePane:
		OutRects[0] = { 0.0f, 0.0f, W, H };
		break;
	case EViewportLayout::TwoPanesHoriz:
		OutRects[0] = { 0.0f, 0.0f, SX, H };
		OutRects[1] = { SX, 0.0f, W - SX, H };
		break;
	case EViewportLayout::TwoPanesVert:
		OutRects[0] = { 0.0f, 0.0f, W, SY };
		OutRects[1] = { 0.0f, SY, W, H - SY };
		break;
	case EViewportLayout::ThreePanesLeft:
		OutRects[0] = { 0.0f, 0.0f, SX, H };
		OutRects[1] = { SX, 0.0f, W - SX, SY };
		OutRects[2] = { SX, SY, W - SX, H - SY };
		break;
	case EViewportLayout::ThreePanesRight:
		OutRects[0] = { SX, 0.0f, W - SX, H };
		OutRects[1] = { 0.0f, 0.0f, SX, SY };
		OutRects[2] = { 0.0f, SY, SX, H - SY };
		break;
	case EViewportLayout::ThreePanesTop:
		OutRects[0] = { 0.0f, 0.0f, W, SY };
		OutRects[1] = { 0.0f, SY, SX, H - SY };
		OutRects[2] = { SX, SY, W - SX, H - SY };
		break;
	case EViewportLayout::ThreePanesBottom:
		OutRects[0] = { 0.0f, SY, W, H - SY };
		OutRects[1] = { 0.0f, 0.0f, SX, SY };
		OutRects[2] = { SX, 0.0f, W - SX, SY };
		break;
	case EViewportLayout::FourPanes2x2:
		OutRects[0] = { 0.0f, 0.0f, SX, SY };
		OutRects[1] = { SX, 0.0f, W - SX, SY };
		OutRects[2] = { 0.0f, SY, SX, H - SY };
		OutRects[3] = { SX, SY, W - SX, H - SY };
		break;
	case EViewportLayout::FourPanesLeft:
		OutRects[0] = { 0.0f, 0.0f, SX, H };
		Column3(SX, W - SX);
		break;
	case EViewportLayout::FourPanesRight:
		OutRects[0] = { SX, 0.0f, W - SX, H };
		Column3(0.0f, SX);
		break;
	case EViewportLayout::FourPanesTop:
		OutRects[0] = { 0.0f, 0.0f, W, SY };
		Row3(SY, H - SY);
		break;
	case EViewportLayout::FourPanesBottom:
		OutRects[0] = { 0.0f, SY, W, H - SY };
		Row3(0.0f, SY);
		break;
	default:
		OutRects[0] = { 0.0f, 0.0f, W, H };
		break;
	}
}

// 최대화까지 반영한 View별 사각형
inline void ComputeViewRects(const FViewportLayoutState& State, const FSplitRatio& Ratio, FVector2 Size, FRect OutRects[4])
{
	if (State.bMaximized)
	{
		for (int32 I = 0; I < 4; ++I)
			OutRects[I] = I == State.MaximizedViewIndex ? FRect{ 0.0f, 0.0f, Size.X, Size.Y } : FRect{};
		return;
	}
	ComputeLayoutRects(State.Layout, Ratio, Size, OutRects);
}

// 칸 사이 경계선. 끌면 Field 비율이 Axis 방향으로 바뀐다.
struct FViewportSplitter
{
	EDragAxis Axis = EDragAxis::Horizontal;   // Horizontal: x 위치를 옮기는 세로선, Vertical: y 위치를 옮기는 가로선
	float Start = 0.0f;                       // 선이 놓인 방향의 시작/끝 (세로선이면 y, 가로선이면 x)
	float End = 0.0f;
	float Position = 0.0f;                    // 세로선이면 x, 가로선이면 y
	ESplitRatioField Field = ESplitRatioField::Horizontal;
};

// 경계선 목록을 채우고 개수를 돌려준다. 최대화 중이거나 한 칸이면 0.
inline int32 ComputeViewportSplitters(const FViewportLayoutState& State, const FSplitRatio& Ratio, FVector2 Size, FViewportSplitter OutSplitters[4])
{
	if (!State.IsShowingMultiplePanes())
		return 0;

	const float W = Size.X, H = Size.Y;
	const float SX = W * Ratio.Horizontal;
	const float SY = H * Ratio.Vertical;
	int32 Count = 0;
	const float T0 = Ratio.Third0, T1 = Ratio.Third1;
	using F = ESplitRatioField;
	auto VLine = [&](float X, float Y0, float Y1, F Field) { OutSplitters[Count++] = { EDragAxis::Horizontal, Y0, Y1, X, Field }; };
	auto HLine = [&](float Y, float X0, float X1, F Field) { OutSplitters[Count++] = { EDragAxis::Vertical, X0, X1, Y, Field }; };

	switch (State.Layout)
	{
	case EViewportLayout::TwoPanesHoriz:    VLine(SX, 0.0f, H, F::Horizontal); break;
	case EViewportLayout::TwoPanesVert:     HLine(SY, 0.0f, W, F::Vertical); break;
	case EViewportLayout::ThreePanesLeft:   VLine(SX, 0.0f, H, F::Horizontal); HLine(SY, SX, W, F::Vertical); break;
	case EViewportLayout::ThreePanesRight:  VLine(SX, 0.0f, H, F::Horizontal); HLine(SY, 0.0f, SX, F::Vertical); break;
	case EViewportLayout::ThreePanesTop:    HLine(SY, 0.0f, W, F::Vertical); VLine(SX, SY, H, F::Horizontal); break;
	case EViewportLayout::ThreePanesBottom: HLine(SY, 0.0f, W, F::Vertical); VLine(SX, 0.0f, SY, F::Horizontal); break;
	case EViewportLayout::FourPanes2x2:     VLine(SX, 0.0f, H, F::Horizontal); HLine(SY, 0.0f, W, F::Vertical); break;
	case EViewportLayout::FourPanesLeft:    VLine(SX, 0.0f, H, F::Horizontal); HLine(H * T0, SX, W, F::Third0); HLine(H * T1, SX, W, F::Third1); break;
	case EViewportLayout::FourPanesRight:   VLine(SX, 0.0f, H, F::Horizontal); HLine(H * T0, 0.0f, SX, F::Third0); HLine(H * T1, 0.0f, SX, F::Third1); break;
	case EViewportLayout::FourPanesTop:     HLine(SY, 0.0f, W, F::Vertical); VLine(W * T0, SY, H, F::Third0); VLine(W * T1, SY, H, F::Third1); break;
	case EViewportLayout::FourPanesBottom:  HLine(SY, 0.0f, W, F::Vertical); VLine(W * T0, 0.0f, SY, F::Third0); VLine(W * T1, 0.0f, SY, F::Third1); break;
	default: break;
	}
	return Count;
}
