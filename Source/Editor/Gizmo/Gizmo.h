#pragma once

#include "Collision/Ray.h"
#include "Math/Transform.h"
#include "Components/SceneComponent.h"
#include "Components/LocalLightComponent.h"

#include "Editor/UnrealWidgetFwd.h"

class UCameraComponent;
class FGizmoRenderer;
class FEditorModeTools;
struct FSceneView;

class FGizmo   // 상태 + 로직
{
public:
	// InModeTools는 이 기즈모가 편집 모드와 좌표계를 읽어 올 모드 툴이다. 기즈모보다 오래 살아 있어야 한다.
	explicit FGizmo(FEditorModeTools* InModeTools);
	~FGizmo();

	// 이 기즈모를 View의 카메라 기준으로 그린다. 씬 위에 올리는 패스 안에서 부른다.
	// UE의 FWidget::Render와 같은 자리다. UE는 Render(View, PDI, ViewportClient)이고 이 엔진은 View만 받는다.
	void Render(const FSceneView* View) const;

	// Gizmo 대상 설정
	void SetTarget(USceneComponent* InTarget, bool InbUseActorBounds = false)
	{ 
		Target = InTarget; 
		bUseActorBounds = InbUseActorBounds; 
	}
	USceneComponent* GetTarget() const { return Target; }

	// 지금의 편집 모드(이동, 회전, 크기)와 좌표계. 기즈모가 갖지 않고 모드 툴에서 읽는다. 바꾸려면 모드 툴에 설정한다.
	// UE에서는 FWidget이 뷰포트 클라이언트의 GetWidgetMode, GetWidgetCoordSystemSpace로 읽고,
	// 뷰포트 클라이언트는 자기 모드 툴에 묻는다. 이 엔진은 아직 기즈모가 뷰포트에 속하지 않아 모드 툴에 바로 묻는다.
	UE::Widget::EWidgetMode GetWidgetMode() const;
	ECoordSystem GetWidgetCoordSystemSpace() const;

	// Active View의 Ray와 행렬만 사용해 Hover·Drag 상태를 갱신한다.
	void Update(const FRay& MouseRay, const FVector2& MousePos,
		const FMatrix& ViewProj, int ScreenW, int ScreenH,
		bool bMouseDown, const FVector& CameraLocation, bool bCameraOrthographic);

	bool IsUsing() const { return DraggingAxis >= 0; }
	int GetHoveredAxis() const { return HoveredAxis; }

	int PickAxis(const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH);

	int PickLinearAxis(const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH);
	int PickRotationAxis(const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH);


	void BeginDrag(int Axis, const FRay& MouseRay, const FVector2& MousePos);
	void UpdateDrag(const FRay& MouseRay, const FVector2& MousePos);
	void EndDrag();

	float ComputeAngleOnPlane(const FVector& Point, int Axis) const;

	inline FTransform GetTransform() const { return Target ? Target->GetTransform() : FTransform(); }
	inline FVector GetLocation() const { return Target ? Target->GetRelativeLocation() : FVector(0, 0, 0); }
	inline FRotator GetRotation() const { return Target ? Target->GetRelativeRotation() : FRotator(0, 0, 0); }
	inline FVector GetScale() const { return Target ? Target->GetRelativeScale3D() : FVector(0, 0, 0); }
	FRotator GetAxisRotation() const;

	FVector GetRenderLocation() const;
	// 원근 View에서만 카메라 거리에 맞춰 크기 보정 위치를 구하고 직교 View는 월드 위치를 유지한다.
	FVector GetRenderLocationForView(const FVector& CameraLocation, bool bCameraOrthographic) const;
	FVector GetCameraLocation() const;
	FVector GetAxisDirection(int Axis) const;
private:
	FEditorModeTools* ModeTools = nullptr;
	FTransform Transform = FTransform();
	USceneComponent* Target = nullptr;
	bool bUseActorBounds = false;

	int HoveredAxis = -1;
	int DraggingAxis = -1;

	FVector2 DragStartMousePos;
	FVector DragStartPoint;
	FVector DragStartLocation;
	FRotator DragStartRotation;
	FVector DragStartScale;
	FVector DragAxisDirection;
	FVector DragStartRenderLocation;

	FVector DragPlaneNormal;

	FVector ViewCameraLocation{};
	bool bViewCameraOrthographic = false;

	float DragStartAngle;
	float RingRadius = 1.0f;

	// Light Component인 경우 Radius를 증감하기 위한 멤버변수와 함수
	float DragStartRadius = 0.0f;
	bool ApplyScaleToLight(float Factor);
	ULocalLightComponent* GetTargetLight() const;

	// 축 메시와 셰이더로 실제 그리기를 맡는 도구. 기즈모마다 하나씩 갖는다.
	TUniquePtr<FGizmoRenderer> Renderer;
};
