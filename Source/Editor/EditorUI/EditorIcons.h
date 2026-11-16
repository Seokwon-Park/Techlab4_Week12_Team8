#pragma once

#include <imgui.h>

// 에디터 아이콘을 ImDrawList 도형으로 직접 그린다.
// 폰트/텍스처 없이 어떤 크기·DPI에서도 선명하고, 속이 채워진 굵은 모양이라 UE5 툴바 느낌에 가깝다.
enum class EEditorIcon : uint8
{
	None,
	Save,
	BrowseContent,
	AddActor,
	Cinematics,
	Play,
	Pause,
	Step,
	Stop,
	MoreVertical,
	ChevronDown,
	Settings,
	Folder,
	NewWindow,
	Check,
	SelectMode,     // 큐브 위 마우스 커서 (Selection Mode)
	Eye,
	EyeClosed,
	Translate,
	Rotate,
	Scale,
	World,          // 지구본 (월드 좌표계)
	Local,          // 큐브 (로컬 좌표계)
	Search,
	Reset,          // 되돌리기 화살표
	Camera,         // 비디오 카메라 (뷰 종류)
	CameraSpeed,    // 속도선 + 화살표
	Lit,            // 빛 받은 구 (뷰 모드)
	LayoutQuad,     // 2x2 칸
	LayoutSingle,   // 한 칸
	Select,         // 화살표 커서 (Select 도구)
};

namespace EditorIcons
{
	// UE5 Play 버튼 초록
	inline constexpr ImU32 PlayGreen = IM_COL32(84, 191, 74, 255);

	// 기본 폰트(13px) 대비 현재 배율. DPI 배율이 들어가 있다.
	float GetScale();

	// Center를 중심으로 Size 크기 정사각형 안에 그린다. Bg는 아이콘 안쪽을 파낼 때 쓰는 바탕색.
	void Draw(ImDrawList* DrawList, EEditorIcon Icon, const ImVec2& Center, float Size, ImU32 Color, ImU32 Bg);

	// 아이콘 버튼. 평소엔 배경이 없고 hover/누름/선택일 때만 칠한다.
	// Label이 있으면 아이콘 오른쪽에 글자를, bDropdown이면 맨 오른쪽에 작은 ▾를 붙인다.
	// Height가 0이면 툴바 기본 높이(28px * 배율)를 쓴다.
	float CalcButtonWidth(bool bHasIcon, const char* Label, bool bDropdown, float Height = 0.0f);
	bool Button(const char* Id, EEditorIcon Icon, ImU32 IconColor, const char* Label = nullptr,
		bool bDropdown = false, bool bSelected = false, float Height = 0.0f, ImGuiButtonFlags Flags = 0);

	// 드롭다운 메뉴 안의 대문자 섹션 제목 (UE5 "MODES", "SPAWN PLAYER AT...").
	void MenuSection(const char* Title);
	// 체크 칸 + 아이콘 + 라벨 한 줄. 눌리면 true.
	// bKeepOpen이면 눌러도 메뉴를 닫지 않는다 (체크 항목 여러 개를 연달아 바꿀 때).
	// Shortcut이 있으면 오른쪽 끝에 흐리게 쓴다. bRadio면 체크 표시 대신 UE5처럼 점(선택이면 밝게)을 찍는다.
	bool MenuRow(const char* Label, EEditorIcon Icon, bool bChecked = false, bool bEnabled = true, bool bKeepOpen = false,
		const char* Shortcut = nullptr, bool bRadio = false);
	// 메뉴 맨 아래 흐린 설명 문단.
	void MenuNote(const char* Text, float WrapWidth = 250.0f);

	// 왼쪽에 돋보기가 있는 검색 칸 (가로를 꽉 채운다). 글자가 바뀌면 true.
	bool SearchBox(const char* Id, char* Buffer, size_t BufferSize, const char* Hint = "Search...");
	// 대소문자 구분 없이 Text 안에 Filter가 있으면 true. Filter가 비어 있으면 항상 true.
	bool MatchesFilter(const char* Text, const char* Filter);
}
