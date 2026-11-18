#pragma once

#include <imgui.h>
#include <functional>

#include "Editor/UnrealWidgetFwd.h"

class FEditorViewportClient;

// 레벨 에디터와 에셋 에디터(스태틱/스켈레탈 메시, 애니메이션, 파티클) 뷰포트가 같이 쓰는 UE5식 뷰포트 UI.
// 위쪽 툴바의 보기 설정 버튼(뷰 종류, 카메라, 뷰 모드, 쇼 플래그), 왼쪽 아래 XYZ 축 표시, Alt+G/J/K/H 단축키.
namespace EditorViewportUI
{
	// 뷰 위쪽 툴바 높이와 그 안의 버튼 높이
	float GetToolbarHeight();
	float GetButtonHeight();

	// 툴바 배경 띠를 그린다.
	void DrawToolbarBackground(const ImVec2& ViewMin, const ImVec2& ViewMax);

	// 알약 모양 묶음 배경 (UE5 툴바 그룹). Begin과 End 사이의 버튼을 감싼다.
	void BeginPill();
	void EndPill();
	// 방금 그린 버튼 바로 아래에 팝업을 연다. bAlignRight면 버튼 오른쪽 끝에 맞춘다.
	void OpenPopupBelow(const char* PopupId, bool bAlignRight);

	// 오른쪽 보기 설정 버튼 묶음: [뷰 종류 ▾][카메라 ▾][뷰 모드 ▾][눈 ▾]
	// CameraSpeed가 있으면 카메라 메뉴에 속도 슬라이더를 넣고 버튼에 값을 쓴다.
	float CalcViewSettingsWidth(const FEditorViewportClient* Client, const float* CameraSpeed, bool bLabels);
	void DrawViewSettings(FEditorViewportClient* Client, float* CameraSpeed, bool bLabels);

	// 이동/회전/크기(그리고 bIncludeSelect면 선택) 도구 버튼. 누른 도구를 SetMode로 알린다.
	void DrawTransformToolButtons(UE::Widget::EWidgetMode Current, const std::function<void(UE::Widget::EWidgetMode)>& SetMode,
		bool bIncludeSelect);

	// 왼쪽 아래 월드 축 표시 (카메라 회전을 따라 돈다)
	void DrawAxisIndicator(ImDrawList* DrawList, const FEditorViewportClient* Client, const ImVec2& ViewMin, const ImVec2& ViewMax);

	// Alt+G 원근, Alt+J/K/H 위/왼쪽/앞, Shift를 더하면 반대쪽. 눌렸으면 Client의 뷰 종류를 바꾼다.
	void HandleViewTypeShortcuts(FEditorViewportClient* Client);

	// 에셋 에디터 뷰포트용 툴바 한 줄. SetGizmoMode가 있으면 왼쪽에 이동/회전/크기 버튼을 둔다.
	// 마우스가 툴바 위에 있거나 툴바 메뉴가 열려 있으면 true (그동안 뷰포트 입력을 막는다).
	bool DrawAssetViewportToolbar(FEditorViewportClient* Client, const ImVec2& ViewMin, const ImVec2& ViewMax,
		UE::Widget::EWidgetMode GizmoMode = UE::Widget::WM_None,
		const std::function<void(UE::Widget::EWidgetMode)>& SetGizmoMode = {});
}
