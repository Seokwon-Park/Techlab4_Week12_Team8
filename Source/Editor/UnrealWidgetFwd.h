#pragma once

// 기즈모(UE에서는 위젯이라고 부른다)의 편집 모드. UE의 UE::Widget::EWidgetMode와 같은 이름이다.
// UE에는 WM_TranslateRotateZ, WM_2D도 있지만 이 엔진은 세 가지만 쓴다.
// 값이 0부터 이어지므로 정수로 바꿔 콤보 상자의 인덱스로 쓸 수 있다.
namespace UE::Widget
{
	enum EWidgetMode
	{
		WM_None = -1,
		WM_Translate,
		WM_Rotate,
		WM_Scale,
		WM_Max,
	};
}

// 기즈모 축을 어느 좌표계에 맞출지. UE의 ECoordSystem과 같은 이름이고 순서(World가 0)도 같다.
enum ECoordSystem
{
	COORD_None = -1,
	COORD_World,
	COORD_Local,
	COORD_Max,
};
