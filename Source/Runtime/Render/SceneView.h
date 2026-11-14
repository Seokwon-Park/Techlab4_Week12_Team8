#pragma once

#include "Core/Types.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Matrix.h"
#include "Math/Frustum.h"
#include "Math/Rect.h"
#include "Asset/LOD/StaticMeshLODSelector.h"
#include "Containers/Array.h"
#include "Render/ShowFlags.h"
#include "Render/SceneRenderOptions.h"
#include <algorithm>

// Immutable description of one camera view for the duration of a render.
struct FSceneView
{
	int32 ViewIndex = 0;
	FVector ViewLocation{};
	FRotator ViewRotation{};
	FVector ViewForward{};
	FMatrix ViewMatrix = FMatrix::Identity;
	FMatrix ProjectionMatrix = FMatrix::Identity;
	FMatrix ViewProjectionMatrix = FMatrix::Identity;
	FFrustumPlanes Frustum{};
	FFrustumCorners FrustumCorners;
	FRect ViewRect{};
	float FieldOfView = 60.0f;
	float AspectRatio = 16.0f / 9.0f;
	float NearClip = 0.1f;
	float FarClip = 10000.0f;
	float OrthoWidth = 10.0f;
	bool bIsPerspective = true;
	bool bIsWireframe = false;
	bool bGameView = false;

	FLinearColor OverlayColor{ 0.0f, 0.0f, 0.0f, 0.0f };   // rgb = 페이드 색, a = FadeAmount
	float LetterboxAmount = 0.0f;                        // 0~1 진행도
	float LetterboxAspect = 2.39f;                       // 목표 화면비
	float VignetteIntensity = 0.0f;

	bool NeedsCameraOverlay(bool bVignette, bool bOverlay) const
	{
		return (bOverlay && (OverlayColor.A > 0.0f || LetterboxAmount > 0.0f))
			|| (bVignette && VignetteIntensity > 0.0f);
	}

	FViewContext ToViewContext() const
	{
		FViewContext Context{};
		Context.ViewIndex = ViewIndex;
		Context.Width = static_cast<uint32>(ViewRect.Width);
		Context.Height = static_cast<uint32>(ViewRect.Height);
		Context.CameraPosition = ViewLocation;
		Context.CameraForward = ViewForward;
		Context.ViewProjection = ViewProjectionMatrix;
		Context.NearZ = NearClip;
		Context.bOrthographic = !bIsPerspective;
		Context.bGameView = bGameView;
		const float ScaleX = ProjectionMatrix.M[1][0];
		const float ScaleY = ProjectionMatrix.M[2][1];
		Context.ProjectionScaleSquared = std::max(ScaleX * ScaleX, ScaleY * ScaleY);
		Context.Prepare();
		return Context;
	}

	FViewContext ToLODViewContext() const
	{
		return ToViewContext();
	}
};

struct FCameraOverlayDebug
{
	bool  bOverride = false;
	float FadeColor[3] = { 0.0f, 0.0f, 0.0f };
	float FadeAmount = 0.0f;
	float LetterboxAmount = 0.0f;
	float LetterboxAspect = 2.39f;
	float VignetteIntensity = 0.0f;
};
inline FCameraOverlayDebug GCameraOverlayDebug;

class UWorld;
struct FSceneViewTargets;

// 한 번에 그리는 뷰들의 묶음. 어디에, 무엇을, 어떤 조건으로 그릴지를 담아 FSceneRenderer에 넘긴다.
// 뷰포트 클라이언트가 Draw에서 만들고 그리기가 끝나면 버린다.
// UE의 FSceneViewFamily와 같은 자리다. RenderTarget, EngineShowFlags, ViewMode, Views는 UE에 있는 이름이다.
struct FSceneViewFamily
{
	// 그릴 대상(색상, 깊이 텍스처). FViewport가 갖고 있는 것을 가리킨다.
	FSceneViewTargets* RenderTarget = nullptr;
	// 그릴 월드. UE는 FSceneInterface* Scene을 갖는다. 이 엔진은 렌더러가 월드에서 씬을 얻으므로 월드를 담는다.
	UWorld* World = nullptr;
	// 무엇을 보여 줄지
	FEngineShowFlags EngineShowFlags;
	// 어떤 방식으로 보여 줄지
	EViewModeIndex ViewMode = VMI_Lit;
	// 품질과 렌더링 방식 설정. UE에는 없는 이 엔진 고유의 멤버다. UE는 이런 값을 콘솔 변수로 둔다.
	FSceneRenderOptions RenderOptions;
	// 이 묶음에 속한 뷰. 지금은 뷰포트마다 하나다. 뷰는 이 묶음보다 오래 살아 있어야 한다.
	TArray<const FSceneView*> Views;
};
