#pragma once

#include "Engine/ViewportClient.h"
#include "Engine/Viewport.h"
#include "Math/Matrix.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Math/Vector2.h"
#include "Render/SceneView.h"
#include "Render/SceneRenderData.h"
#include "Render/SceneRenderOptions.h"
#include "Render/Texture2D.h"

#include <functional>

class UWorld;
class FGizmo;
class AActor;
class FPreviewScene;
class FRenderer;
struct FBoxSphereBounds;
struct FScenePassCallbacks;


// 뷰포트 투영 타입
enum class ELevelViewportType
{
	Perspective,
	Top,
	Front,
	Right,
};

// 에디터 뷰포트 클라이언트
class FEditorViewportClient : public FViewportClient {
public:
	FEditorViewportClient();
	explicit FEditorViewportClient(FPreviewScene& InPreviewScene);
	virtual ~FEditorViewportClient() override = default;

	// 월드 조회 및 설정
	virtual UWorld* GetWorld() const override;
	void SetWorld(UWorld* InWorld);

	// 입력 이벤트
	virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y) override;
	virtual bool InputKey(FViewport* Viewport, int32 Key, bool bDown) override;
	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, EGameInputAxis AxisKey, float Delta, float DeltaTime) override;

	// 뷰포트 상태
	int32 GetViewIndex() const { return ViewIndex; }
	void SetViewIndex(int32 InIndex) { ViewIndex = InIndex; }

	ELevelViewportType GetViewportType() const { return ViewportType; }
	void SetViewportType(ELevelViewportType InType);

	bool IsPerspective() const { return bIsPerspective; }
	void SetIsPerspective(bool bPerspective) { bIsPerspective = bPerspective; }

	bool IsRealtime() const { return bIsRealtime; }
	void SetRealtime(bool bRealtime) { bIsRealtime = bRealtime; }

	// 카메라 변환
	const FVector& GetViewLocation() const { return ViewLocation; }
	void SetViewLocation(const FVector& InLocation) { ViewLocation = InLocation; }

	const FRotator& GetViewRotation() const { return ViewRotation; }
	void SetViewRotation(const FRotator& InRotation) {
		ViewRotation = InRotation;
	}

	float GetViewFOV() const { return ViewFOV; }
	void SetViewFOV(float InFOV) { ViewFOV = InFOV; }

	float GetOrthoWidth() const { return OrthoWidth; }
	void SetOrthoWidth(float InWidth) { OrthoWidth = InWidth; }

	float GetAspectRatio() const { return AspectRatio; }
	void SetAspectRatio(float InAspectRatio) { AspectRatio = InAspectRatio; }

	// 카메라 입력 처리
	void TickInput(float DeltaTime, bool bCaptured, bool bHovered,
		int32 WheelDelta, float MoveSpeed, float MouseSensitivity);

	// 기즈모 조작과 피킹 처리
	void
		UpdateGizmoAndPicking(FGizmo* InGizmo, const FVector2& LocalMousePosition,
			const std::function<void(AActor*)>& OnSelectActor);

	// 행렬 계산
	FMatrix GetViewMatrix() const;
	FMatrix GetPerspectiveMatrix() const;
	FMatrix GetOrthogonalMatrix() const;
	FMatrix GetProjectionMatrix() const;
	FMatrix GetViewProjectionMatrix() const;

	// 카메라 전방 벡터
	FVector GetViewForward() const;
	void FocusOnBounds(const FBoxSphereBounds& Bounds);

	// 씬 뷰 생성
	FSceneView CalcSceneView(const FRect& InViewRect) const;
	FSceneView CalcSceneView() const;

	// 이 뷰포트의 월드를 자기 카메라로 InViewport의 타깃에 그린다. FViewport::Draw가 부른다.
	// UE의 FEditorViewportClient::Draw(FViewport*, FCanvas*)와 같다.
	// 무엇을 어떻게 보여 줄지는 자기 EngineShowFlags에서 읽는다. 에디터 뷰포트는 Canvas를 쓰지 않는다.
	// 씬을 그리는 도중에 아래의 두 번째 Draw를 부른다.
	void Draw(FViewport* InViewport, FUIRenderer* Canvas) override;

	// 씬 위에 올릴 에디터 요소(선, 그리드, 기즈모 등)를 등록한다. 기본 구현은 아무것도 올리지 않는다.
	// 레벨 에디터와 에셋 에디터의 클라이언트가 이 함수만 재정의한다.
	// UE의 FEditorViewportClient::Draw(const FSceneView*, FPrimitiveDrawInterface*)와 같은 자리다.
	// UE는 PDI로 바로 그리지만, 이 엔진은 그릴 시점에 불릴 함수를 Callbacks에 채운다.
	virtual void Draw(const FSceneView* View, FScenePassCallbacks& Callbacks);

	// 화면 역투영 레이 생성
	FRay DeprojectScreenToWorld(const FVector2& ScreenPos) const;

	// 뷰포트 영역 및 타깃
	const FRect& GetRect() const { return Rect; }
	void SetRect(const FRect& InRect) { Rect = InRect; }

	bool IsActive() const { return bActive; }
	void SetActive(const bool bInActive) { bActive = bInActive; }

	// 이 클라이언트가 그리는 뷰포트의 크기. 뷰포트가 아직 없으면 0이다.
	uint32 GetWidth() const { return Viewport ? static_cast<uint32>(Viewport->GetSizeXY().X) : 0; }
	uint32 GetHeight() const { return Viewport ? static_cast<uint32>(Viewport->GetSizeXY().Y) : 0; }

	FSceneRenderData& GetRenderData() { return RenderData; }

	bool IsWireframe() const { return ViewModeIndex == VMI_Wireframe; }
	bool IsSceneDepth() const { return ViewModeIndex == VMI_SceneDepth; }
	bool IsWorldNormal() const { return ViewModeIndex == VMI_WorldNormal; }
	bool IsLightCulling() const { return ViewModeIndex == VMI_LightCulling; }
	bool IsCascade() const { return ViewModeIndex == VMI_Cascade; }
	bool IsShadowMask() const { return ViewModeIndex == VMI_ShadowMask; }
	// 이 뷰포트의 뷰 모드. UE의 FEditorViewportClient::GetViewMode, SetViewMode와 같다.
	EViewModeIndex GetViewMode() const { return ViewModeIndex; }
	void SetViewMode(const EViewModeIndex InViewModeIndex) { ViewModeIndex = InViewModeIndex; }
	float GetMaxRange() const { return MaxRange; }
	void SetMaxRange(float InMaxRange) { MaxRange = InMaxRange; }

	// 뷰포트 크기가 바뀔 때 FViewport::UpdateViewportRHI가 부른다. 종횡비와 직교 배율을 새 크기에 맞춘다.
	// 불리는 시점에 InViewport->GetSizeXY()는 아직 이전 크기다.
	void Resize(FViewport* InViewport, uint32 NewWidth, uint32 NewHeight) override;

	// 이 클라이언트가 그리는 뷰포트. 그릴 대상(색상, 깊이 텍스처)과 크기는 뷰포트가 갖는다.
	// 뷰포트를 만든 쪽(뷰포트 패널, 에셋 에디터)이 채운다.
	// UE의 FEditorViewportClient::Viewport와 같은 자리이고 UE에서도 public 멤버다.
	FViewport* Viewport = nullptr;

protected:
	// 프리뷰 장면은 클라이언트보다 오래 살아 있어야 합니다.
	FPreviewScene* PreviewScene = nullptr;
	UWorld* World = nullptr;
	int32 ViewIndex = 0;
	ELevelViewportType ViewportType = ELevelViewportType::Perspective;
	bool bIsPerspective = true;
	bool bIsRealtime = true;
	bool bActive = false;
	// 이 뷰포트의 뷰 모드. UE는 원근용과 직교용을 따로 갖는다(PerspViewModeIndex, OrthoViewModeIndex).
	EViewModeIndex ViewModeIndex = VMI_Lit;

	FRect Rect{};
	FSceneRenderData RenderData;
	// 이 뷰포트에 무엇을 보여 줄지 켜고 끄는 스위치. Draw가 뷰 묶음(FSceneViewFamily)에 담아 렌더러에 넘긴다.
	// UE의 FEditorViewportClient::EngineShowFlags와 같다.
	FEngineShowFlags EngineShowFlags;
	// 그림자 해상도, 렌더링 경로 같은 품질과 방식 설정. 쇼 플래그와 함께 뷰 묶음에 담는다.
	// UE에는 없는 이 엔진 고유의 멤버다. UE는 이런 값을 콘솔 변수로 둔다.
	FSceneRenderOptions RenderOptions;

	FVector ViewLocation = FVector(0.0f, 0.0f, 0.0f);
	FRotator ViewRotation = FRotator(0.0f, 0.0f, 0.0f);
	float ViewFOV = 60.0f;
	float AspectRatio = 1.777778f;
	float NearClip = 0.1f;
	float FarClip = 10000.0f;
	float OrthoWidth = 10.0f;
	float MaxRange = 50.0f;
};
