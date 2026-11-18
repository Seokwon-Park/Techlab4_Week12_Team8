#include "EnginePCH.h"
#include "Editor/Viewports/EditorViewportToolbar.h"
#include "SkeletalMeshViewportPanel.h"
#include "Editor/SkeletalMeshEditor/SkeletalMeshEditor.h"

FSkeletalMeshViewportPanel::FSkeletalMeshViewportPanel(FSkeletalMeshEditor& InEditor)
	: FSkeletalMeshEditorPanel(InEditor, "Viewport")
{
}

void FSkeletalMeshViewportPanel::DrawContent()
{

	constexpr int32 GizmoModeCount = 3;   // 이동, 회전, 크기 (툴바 버튼과 Space 순환)

	FEditorViewportClient* Client = Editor.GetViewportClient();
	const ImVec2 Size = ImGui::GetContentRegionAvail();
	if (!Client || Size.x < 1.0f || Size.y < 1.0f)
	{
		return;
	}

	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const ImVec2 ViewMax(Origin.x + Size.x, Origin.y + Size.y);
	if (FTexture2D* Target = Client->Viewport ? Client->Viewport->GetRenderTargetTexture() : nullptr; Target && Target->GetSRV())
	{
		ImGui::GetWindowDrawList()->AddImage(Target->GetSRV(), Origin, ViewMax);
	}

	// UE5식 뷰포트 툴바(기즈모 도구, 뷰 종류, 카메라, 뷰 모드, 쇼 플래그)와 왼쪽 아래 축 표시.
	// 입력 영역보다 먼저 그려야 툴바 버튼이 마우스를 먼저 받는다. 툴바 위에서는 뷰포트 입력을 막는다.
	const bool bToolbarBusy = EditorViewportUI::DrawAssetViewportToolbar(Client, Origin, ViewMax, Editor.GetGizmoMode(), [this](UE::Widget::EWidgetMode Mode) { Editor.SetGizmoMode(Mode); });

	// 이미지 영역을 입력 항목으로 등록하여 드래그 중에도 카메라 조작을 유지한다.
	ImGui::SetCursorScreenPos(Origin);
	ImGui::InvisibleButton("##SkeletalMeshViewport", Size,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);


	const bool bHovered = ImGui::IsItemHovered() && !bToolbarBusy;
	if (bHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
		ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
	{
		ImGui::SetWindowFocus();
	}

	const bool bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (bFocused && bHovered)
		EditorViewportUI::HandleViewTypeShortcuts(Client);
	if (bFocused && bHovered && ImGui::IsKeyPressed(ImGuiKey_F, false))
	{
		Editor.FocusMesh();
	}


	// 분리된 플랫폼 창의 입력도 받을 수 있도록 ImGui 입력을 사용합니다.
	//
	// TODO: 여기서도 FInputSystem을 사용할 수 있도록, 구조 개선하기
	// ImGui가 생성하는 HWND에 직접 WndProc을 연결하고 FInputSystem에 전달시켜야함

	const ImGuiIO& IO = ImGui::GetIO();
	const bool bMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	const bool bCameraDragging = ImGui::IsMouseDown(ImGuiMouseButton_Right);
	const FViewportPointerInput PointerInput
	{
		.MouseUV = FVector2((IO.MousePos.x - Origin.x) / Size.x, (IO.MousePos.y - Origin.y) / Size.y),
		.bHovered = bHovered,
		.bFocused = bFocused && !IO.AppFocusLost,
		.bMousePressed = bHovered && !bCameraDragging && ImGui::IsMouseClicked(ImGuiMouseButton_Left),
		.bMouseDown = bMouseDown,
		.bCancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false),
	};

	FViewportCameraInput Input
	{
		.bCaptured		= bFocused && ImGui::IsItemActive() && bCameraDragging && !bMouseDown && !Editor.IsGizmoDragging(),
		.bAllowWheel	= bHovered && !bMouseDown && !Editor.IsGizmoDragging(),
		.bForward		= ImGui::IsKeyDown(ImGuiKey_W),
		.bBackward		= ImGui::IsKeyDown(ImGuiKey_S),
		.bRight			= ImGui::IsKeyDown(ImGuiKey_D),
		.bLeft			= ImGui::IsKeyDown(ImGuiKey_A),
		.bUp			= ImGui::IsKeyDown(ImGuiKey_E),
		.bDown			= ImGui::IsKeyDown(ImGuiKey_Q),
		.MouseX			= IO.MouseDelta.x,
		.MouseY			= IO.MouseDelta.y,

		// ImGui의 휠 단위를 기존 카메라 이동 함수가 사용하는 Windows 휠 단위로 변환합니다.
		.Wheel			= IO.MouseWheel * 120.0f,
	};

	// 키보드로 Gizmo 키 변경
	if (!bCameraDragging)
	{
		if (ImGui::IsKeyDown(ImGuiKey_W))
		{
			Editor.SetGizmoMode(UE::Widget::WM_Translate);
		}
		if (ImGui::IsKeyDown(ImGuiKey_E))
		{
			Editor.SetGizmoMode(UE::Widget::WM_Rotate);
		}
		if (ImGui::IsKeyDown(ImGuiKey_R))
		{
			Editor.SetGizmoMode(UE::Widget::WM_Scale);
		}

		// 뷰포트에서 Space를 누를 때마다 이동, 회전, 크기 모드를 순환합니다.
		if (bFocused && bHovered && !IO.WantTextInput && !IO.AppFocusLost &&
			!Editor.IsGizmoDragging() && ImGui::IsKeyPressed(ImGuiKey_Space, false))
		{
			const int32 NextMode = (static_cast<int32>(Editor.GetGizmoMode()) + 1) % GizmoModeCount;
			Editor.SetGizmoMode(static_cast<UE::Widget::EWidgetMode>(NextMode));
		}
	}

	// 렌더 타깃 변경은 Editor의 다음 Render에서 수행합니다.
	uint32 Width = static_cast<uint32>(Size.x);
	uint32 Height = static_cast<uint32>(Size.y);
	Editor.SetViewportState(Width, Height, Input, PointerInput);
}
