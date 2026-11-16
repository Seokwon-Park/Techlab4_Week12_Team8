#pragma once

#include <imgui.h>
#include <imgui_internal.h>

class IEditorPanel
{
public:
	virtual ~IEditorPanel() = default;
	virtual bool Init() = 0;
	virtual void Tick(float DeltaTime) = 0;
	virtual void OnRender() = 0;

	bool IsOpen() const { return bIsOpen; }
	void SetOpen(bool bOpen) { bIsOpen = bOpen; }
	virtual const char* GetPanelName() const = 0;

private:
	bool bIsOpen = true;
};

// 도킹 탭 줄의 높이는 Begin/DockSpace 시점의 FramePadding.y로 정해진다.
// 위젯 패딩은 촘촘하게 두고 탭만 키우려고, 그 두 호출 동안에만 세로 패딩을 키운다.
namespace EditorPanelUI
{
	// 탭 세로 패딩 (DPI 배율 1 기준 px). 탭 높이 = 글자 크기 + 2 * 이 값.
	inline constexpr float TabPaddingY = 7.0f;

	inline void PushTabPadding()
	{
		const ImGuiStyle& Style = ImGui::GetStyle();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Style.FramePadding.x, TabPaddingY * Style.FontScaleDpi));
	}

	inline void PopTabPadding() { ImGui::PopStyleVar(); }

	// ImGui::Begin 대신 쓴다. 패널 내용은 원래 패딩으로 그려진다.
	inline bool BeginPanel(const char* Name, bool* bOpen = nullptr, ImGuiWindowFlags Flags = 0)
	{
		PushTabPadding();
		const bool bVisible = ImGui::Begin(Name, bOpen, Flags);
		PopTabPadding();
		return bVisible;
	}
}