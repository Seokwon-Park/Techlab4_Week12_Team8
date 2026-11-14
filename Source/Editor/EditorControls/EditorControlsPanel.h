#pragma once
#include "Editor/EditorUI/EditorPanel.h"

class FGizmo;
class FViewportsPanel;
class UWorld;

// 레벨 에디터의 조작 패널. 위에서부터 프레임 시간, 액터 생성, 기즈모 모드, 뷰포트 카메라, 스켈레탈 메시 순서로 그린다.
// 섹션마다 Draw 함수가 하나씩 있고 OnRender가 순서대로 부른다.
class FEditorControlsPanel : public IEditorPanel
{
public:
	bool Init() override;
	// 프레임 시간을 기억하고 Space 키로 기즈모 모드를 바꾼다.
	void Tick(float InDeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override { return "Editor Controls"; }

	// 기즈모가 없으면 기즈모 섹션을 그리지 않는다.
	void SetGizmo(FGizmo* InGizmo) { Gizmo = InGizmo; }
	// 액터를 생성하고 카메라를 읽어 올 월드. PIE를 시작하고 끝낼 때 엔진이 바꿔 준다.
	void SetWorld(UWorld* InWorld) { World = InWorld; }
	// 뷰포트 섹션이 활성 뷰포트를 찾을 때 쓴다.
	void SetViewportsPanel(FViewportsPanel* InPanel) { ViewportsPanel = InPanel; }

private:
	// 액터 종류 콤보와 Spawn 버튼, 라이트와 데칼 생성 버튼
	void DrawSpawnSection();
	// 기즈모의 편집 모드와 좌표계
	void DrawGizmoSection();
	// 활성 뷰포트의 카메라 투영과 위치, 회전
	void DrawCameraProperties();
	// GPU 스키닝 토글과 본 테스트용 도구
	void DrawSkeletalMeshSection();

	static constexpr float SectionGap = 10.0f;
	static constexpr float SubsectionGap = 4.0f;

	UWorld* World = nullptr;
	FViewportsPanel* ViewportsPanel = nullptr;
	FGizmo* Gizmo = nullptr;

	// 마지막 Tick의 프레임 시간. FPS 표시에 쓴다.
	float DeltaTime = 1.0f;
	// 액터 종류 콤보에서 고른 항목
	int32 SelectedActorIndex = 0;
};
