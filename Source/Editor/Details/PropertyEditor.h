#pragma once

#include <functional>

struct ImFont;
class UObject;
class UStruct;
class FProperty;

namespace FPropertyEditor
{
	// DetailsPanel.cpp의 DrawProperty를 그대로 옮긴다 (값이 바뀌면 PostEditChangeProperty 호출).
	void DrawProperty(UObject* Object, const FProperty& Property, ImFont* CustomFont = nullptr);
	// 기반 클래스부터 CPF_Edit 속성을 CollapsingHeader로 묶어 그린다 (DetailsPanel의 루프를 옮김).
	void DrawObjectProperties(UObject* Object, ImFont* CustomFont = nullptr);

	// USTRUCT 멤버 표시 규칙 (UE의 EditCondition / IPropertyTypeCustomization을 줄인 것).
	// 그 구조체를 그릴 때 멤버마다 불러, 숨길 멤버는 bVisible = false, 라벨을 바꾸려면 OutLabel을 채운다.
	// StructValue는 그리고 있는 구조체 값의 주소다 (예: FRawDistributionFloat*).
	struct FStructMemberDisplay
	{
		bool bVisible = true;
		const char* Label = nullptr;   // nullptr이면 속성 이름
	};
	using FStructMemberRule = std::function<FStructMemberDisplay(const void* StructValue, const FProperty& Member)>;
	void RegisterStructMemberRule(const UStruct* Struct, FStructMemberRule Rule);
}
// DetailsPanel.cpp는 이 헤더를 include하고 원래 함수 대신 FPropertyEditor::를 부르도록 바꾼다.
