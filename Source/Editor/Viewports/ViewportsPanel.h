#pragma once

#include "Editor/EditorUI/EditorPanel.h"
#include "Editor/Viewports/LevelEditorViewport.h"
#include "Editor/LevelViewportLayout.h"
#include <functional>

class FGameViewportClient;

class FViewportsPanel : public IEditorPanel {
public:
	// 활성 뷰포트 인덱스 설정
	void SetActiveViewIndex(const int32 InIndex) { ActiveViewIndex = InIndex; }
	int32 GetActiveViewIndex() const { return ActiveViewIndex; }

	// 네 View의 렌더 타깃과 UI 제어 상태를 초기화한다.
	bool Init() override;
	// 프레임 입력에서 Splitter Drag와 View별 UI 요청을 수집한다.
	void Tick(float DeltaTime) override;
	// 각 View Texture와 Splitter·Layout 조작 UI를 ImGui 패널에 그린다.
	void OnRender() override;
	const char *GetPanelName() const override { return "Viewports"; }

	// Core가 계산한 View Rect에 맞춰 해당 클라이언트의 활성 상태와 크기를 갱신한다.
	void SetView(int32 ViewIndex, const FRect &Rect, bool bActive);

	FVector2 GetContentSize() const { return {ContentSize.x, ContentSize.y}; }
	// 패널 내용 영역의 좌상단 (ImGui 멀티 뷰포트라 OS 화면 좌표)
	FVector2 GetContentOrigin() const { return {ContentOrigin.x, ContentOrigin.y}; }
	FVector2 GetLocalMousePosition() const;
	bool IsHovered() const;
	bool IsFocused() const { return bFocused; }

	// 분할선을 끈 만큼 비율을 바꿔 InOutRatio에 쓴다. 바뀐 게 있으면 true.
	bool ConsumeSplitRatio(FSplitRatio& InOutRatio);

	// UI에서 발생한 레이아웃 변경(배치, 최대화) 요청을 한 번 소비하도록 반환한다.
	bool ConsumeLayoutRequest(FViewportLayoutState &OutLayout);
	// 엔진이 매 프레임 지금 배치와 분할 비율을 알려 준다. 분할선과 Layout 메뉴가 이것을 쓴다.
	void SetLayoutState(const FViewportLayoutState& InLayout, const FSplitRatio& InRatio);
	// Immersive View(F11): 뷰포트 패널이 에디터 창 전체를 덮는다.
	bool IsImmersive() const { return bImmersive; }

	// 뷰포트 클라이언트 조회
	FLevelEditorViewportClient* GetViewportClient(const int32 ViewIndex) const
	{
			if (ViewIndex >= 0 && ViewIndex < 4)
			{
					return ViewportClients[ViewIndex].get();
			}
			return nullptr;
	}
	// View 하나의 뷰포트. 엔진이 이 뷰포트의 Draw를 불러 그리게 한다.
	FViewport* GetViewport(const int32 ViewIndex) const
	{
		return (ViewIndex >= 0 && ViewIndex < 4) ? Viewports[ViewIndex].get() : nullptr;
	}
	void SetGameViewportClient(int32 ViewIndex, FGameViewportClient* InGameClient) {
		if (ViewIndex < 0 || ViewIndex >= 4)
			return;
		GameViewportIndex = ViewIndex;
		GameViewportClient = InGameClient;
	}
	void ClearGameViewportClient() {
		GameViewportClient = nullptr;
		GameViewportIndex = -1;
	}

	void SetShowNoCamera(bool bValue) { bShowNoCamera = bValue; }

	// 뷰포트 툴바의 카메라 속도 메뉴가 바꿀 값 (에디터 설정의 CameraSpeed).
	void SetCameraSpeedTarget(float* InCameraSpeed) { CameraSpeed = InCameraSpeed; }
	// 뷰 위쪽 툴바 높이. 통계 오버레이는 이만큼 아래에서 시작한다.
	static float GetToolbarHeight() { return 30.0f * ImGui::GetFontSize() / 13.0f; }

private:
	int32 ActiveViewIndex = 0;
	bool bShowNoCamera = false;

	// 통계 정보 출력
	// 뷰 위쪽의 UE5식 툴바 (기즈모 도구, 뷰 종류, 카메라, 뷰 모드, 쇼 플래그, 레이아웃)
	void DrawViewportToolbar(int32 ViewIndex, FEditorViewportClient* Client, const ImVec2& ViewMin, const ImVec2& ViewMax);

	void DrawStatOverlay(ImDrawList* DrawList, const ImVec2& ViewMin, FEditorViewportClient* Client) const;
	// 뷰 왼쪽 아래 월드 축 표시 (UE5 뷰포트 좌하단 XYZ). 카메라 회전을 따라 돈다.
	static void DrawAxisIndicator(ImDrawList* DrawList, const FEditorViewportClient* Client, const ImVec2& ViewMin, const ImVec2& ViewMax);
	// 칸 사이 경계선을 그리고 끌기를 받는다.
	void DrawSplitters(ImDrawList* DrawList);
	// UE5 Layout 메뉴 (One~Four Panes 그림 버튼 + Immersive View / Maximize Viewport)
	void DrawLayoutMenu(int32 ViewIndex);
	// 배치를 작은 그림으로 그린다 (Layout 메뉴 버튼과 툴바 버튼 아이콘)
	static void DrawLayoutGlyph(ImDrawList* DrawList, EViewportLayout Layout, const ImVec2& Min, const ImVec2& Size, ImU32 Color);
	void RequestLayout(const FViewportLayoutState& InLayout);

	TUniquePtr<FLevelEditorViewportClient> ViewportClients[4]{};
	// View마다 하나씩 있는 뷰포트. 그릴 대상(텍스처)과 크기를 갖고, 그리는 일은 위의 클라이언트에게 맡긴다.
	// UE에서는 뷰포트 위젯(SEditorViewport)이 FSceneViewport를 갖는다. 이 엔진은 그 자리가 이 패널이다.
	TUniquePtr<FViewport> Viewports[4]{};
	// PIE 월드 컨텍스트가 소유하며, 세션 종료 전에 비운다.
	FGameViewportClient* GameViewportClient = nullptr;
	int32 GameViewportIndex = -1;
	ImVec2 ContentOrigin{};
	ImVec2 ContentSize{1.0f, 1.0f};
	bool bHovered = false;
	bool bFocused = false;
	bool bStatResetButtonVisible = false;
	bool bToolbarHovered = false;
	float* CameraSpeed = nullptr;
	ImVec2 StatResetButtonMin{};
	ImVec2 StatResetButtonMax{};
	// 분할선 드래그로 쌓인 비율 변화량 (ESplitRatioField 순서, 패널 크기 대비)
	float PendingRatioDelta[static_cast<int32>(ESplitRatioField::Count)]{};
	// 누른 뒤 커서가 Splitter 영역을 벗어나도 버튼을 놓을 때까지 드래그를 유지한다.
	uint8 DraggingSplitterMask = 0;   // 끌고 있는 비율 값 (1 << ESplitRatioField). 교차점을 잡으면 둘 다
	FViewportLayoutState CurrentLayout;
	FSplitRatio CurrentSplitRatio;
	bool bHasLayoutRequest = false;
	FViewportLayoutState RequestedLayout;
	bool bImmersive = false;

};
