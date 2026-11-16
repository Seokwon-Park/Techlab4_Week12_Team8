#pragma once

#include "Editor/UnrealWidgetFwd.h"

// 레벨 에디터의 편집 모드. UE의 FEditorModeID(Selection, Landscape, Foliage, Modeling ...)와 같은 자리다.
// 이 엔진에는 지금 Selection(액터 선택·기즈모 편집)만 있다. 새 모드를 만들면 여기에 더하고
// 툴바의 모드 메뉴(FLevelEditorToolbar::DrawSelectionModeMenu)에 줄을 추가한다.
enum class EEditorModeID : uint8
{
	Selection,
};

// 에디터 하나의 편집 상태를 갖는다. 활성 편집 모드, 기즈모의 편집 모드(이동, 회전, 크기)와 좌표계(월드, 로컬)다.
// 기즈모는 이 값을 직접 갖지 않고 자기가 속한 모드 툴에서 읽는다.
// UE의 FEditorModeTools와 같은 자리이고, UE도 EditorModeManager.h에 둔다.
// UE의 FEditorModeTools는 활성 에디터 모드 목록과 기즈모 위치 계산도 맡는다.
class FEditorModeTools
{
public:
	// 활성 편집 모드 (UE의 ActivateMode / IsModeActive)
	EEditorModeID GetActiveMode() const { return ActiveMode; }
	void ActivateMode(EEditorModeID InMode) { ActiveMode = InMode; }
	bool IsModeActive(EEditorModeID InMode) const { return ActiveMode == InMode; }

	// 기즈모의 편집 모드
	UE::Widget::EWidgetMode GetWidgetMode() const { return WidgetMode; }
	void SetWidgetMode(UE::Widget::EWidgetMode InWidgetMode) { WidgetMode = InWidgetMode; }

	// 기즈모 축의 좌표계
	ECoordSystem GetCoordSystem() const { return CoordSystem; }
	void SetCoordSystem(ECoordSystem NewCoordSystem) { CoordSystem = NewCoordSystem; }

private:
	EEditorModeID ActiveMode = EEditorModeID::Selection;
	UE::Widget::EWidgetMode WidgetMode = UE::Widget::WM_Translate;
	// UE의 기본값은 COORD_World다. 이 엔진은 지금까지의 동작을 유지하려고 로컬로 시작한다.
	ECoordSystem CoordSystem = COORD_Local;
};

// 레벨 에디터의 모드 툴. 레벨 뷰포트의 기즈모와 에디터 컨트롤 패널이 같이 쓴다.
// UE의 GLevelEditorModeTools()와 같다. 에셋 에디터는 이것을 쓰지 않고 각자 FEditorModeTools를 하나씩 갖는다.
FEditorModeTools& GLevelEditorModeTools();
