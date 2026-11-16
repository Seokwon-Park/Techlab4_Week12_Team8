#include "EnginePCH.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include <cmath>
#include "Math/Frustum.h"
#include "Render/RenderCommand.h"
#include "UI/UserWidget.h"
#include "UI/UIRenderer.h"
#include "Render/Renderer.h"
#include "Render/SceneRenderer.h"
#include "Launch/LaunchEngineLoop.h"

FGameViewportClient::FGameViewportClient()
{
}

void FGameViewportClient::Init(FWorldContext& InWorldContext, UEngine* InEngine)
{
	SetWorld(InWorldContext.World());
	GameInstance = InWorldContext.OwningGameInstance;
	Engine = InEngine;
}

UWorld* FGameViewportClient::GetWorld() const
{
	return World;
}

void FGameViewportClient::MouseMove(FViewport* Viewport, int32 X, int32 Y)
{
	HandleUIMouseMove(X, Y);
}

bool FGameViewportClient::InputKey(FViewport* Viewport, int32 Key, bool bDown)
{
	if (HandleUIKey(Key, bDown))
		return true;
	if (bDown && Key == static_cast<int32>(EKeyCode::Escape))
	{
		bExitRequested = true;
		return true;
	}

	if (World)
	{
		if (APlayerController* PC = World->GetPlayerController())
		{
			PC->InputKey(Key, bDown);
			return true;
		}
	}
	return false;
}

bool FGameViewportClient::InputAxis(FViewport* Viewport, int32 ControllerId, EGameInputAxis AxisKey, float Delta, float DeltaTime)
{
	if (HandleUIAxis(AxisKey, Delta))
		return true;

	if (World)
	{
		if (APlayerController* PC = World->GetPlayerController())
		{
			PC->InputAxis(AxisKey, Delta);
			return true;
		}
	}
	return false;
}

void FGameViewportClient::LostFocus()
{
	if (World)
	{
		if (APlayerController* PC = World->GetPlayerController())
			PC->ClearInputState();
	}
}

bool FGameViewportClient::ScreenToWorldRay(
	float X, float Y, FRay* OutRay)
{
	assert(OutRay);

	const FRect& Rect = GetRect();
	const uint32 Width = GetWidth();
	const uint32 Height = GetHeight();

	if (!CameraComponent ||
		Rect.Width <= 0.0f || Rect.Height <= 0.0f ||
		Width == 0 || Height == 0)
	{
		return false;
	}

	if (X < 0.0f || Y < 0.0f ||
		X >= Rect.Width || Y >= Rect.Height)
	{
		return false;
	}

	const float RenderX = X * Width / Rect.Width;
	const float RenderY = Y * Height / Rect.Height;

	*OutRay = CameraComponent->DeProjection(
		FVector2(RenderX, RenderY),
		static_cast<float>(Width),
		static_cast<float>(Height));

	return true;
}

UCameraComponent* FGameViewportClient::FindPlayerCamera() const
{
	if (!World) return nullptr;
	if (APawn* Pawn = World->GetPlayerPawn())
	{
		if (ADefaultPawn* DefaultPawn = Cast<ADefaultPawn>(Pawn))
			if (UCameraComponent* Camera = DefaultPawn->GetCameraComponent()) return Camera;
		for (UActorComponent* Component : Pawn->GetComponents())
			if (UCameraComponent* Camera = Cast<UCameraComponent>(Component)) return Camera;
	}
	return nullptr;
}

void FGameViewportClient::SetWorld(UWorld* InWorld)
{
	if (World == InWorld) return;
	LostFocus();
	RenderData.ResetScene();
	CameraComponent = nullptr;
	bExitRequested = false;
	World = InWorld;
}

FSceneView FGameViewportClient::CalcSceneView(const FRect& InViewRect)
{
	FSceneView OutView;
	OutView.ViewIndex = 0;
	OutView.bGameView = true;
	OutView.ViewRect = InViewRect;

	// 종횡비는 넘겨받은 영역에서 구한다. 타깃 크기는 뷰포트가 맡으므로 여기서 바꾸지 않는다.
	const float Aspect = std::max(InViewRect.Width, 1.0f) / std::max(InViewRect.Height, 1.0f);

	if (!CameraComponent && World)
	{
		CameraComponent = FindPlayerCamera();

		if (!CameraComponent && World->GetMainCamera())
			CameraComponent = World->GetMainCamera()->GetCameraComponent();
	}

	if (World)
	{
		APlayerController* PC = World->GetPlayerController();
		APlayerCameraManager* PCM = PC ? PC->GetCameraManager() : nullptr;
		if (PCM && PCM->HasCameraCache())
		{
			FMinimalViewInfo POV = PCM->GetCameraCachePOV();
			POV.AspectRatio = Aspect;

			// 위치·회전·행렬
			OutView.ViewLocation = POV.Location;
			OutView.ViewRotation = POV.Rotation;
			OutView.ViewForward = POV.Rotation.Quaternion().GetForwardVector();
			OutView.ViewMatrix = POV.CalculateViewMatrix();
			OutView.ProjectionMatrix = POV.CalculateProjectionMatrix();
			OutView.ViewProjectionMatrix = OutView.ViewMatrix * OutView.ProjectionMatrix;
			// 렌즈 값
			OutView.FieldOfView = POV.FOV;
			OutView.AspectRatio = Aspect;
			OutView.NearClip = POV.NearClip;
			OutView.FarClip = POV.FarClip;
			OutView.OrthoWidth = POV.OrthoWidth;
			OutView.bIsPerspective = !POV.bIsOrthogonal;
			OutView.bIsWireframe = false;

			// 프러스텀 후 반환
			OutView.Frustum = ExtractFrustumPlanes(OutView.ViewProjectionMatrix);
			OutView.FrustumCorners = GenerateFrustumCorners(OutView.ViewProjectionMatrix.Inverse());
			if (PCM->bEnableFading)
			{
				OutView.OverlayColor = PCM->FadeColor;
				OutView.OverlayColor.A = FMath::Clamp(PCM->FadeAmount, 0.0f, 1.0f);
			}
			OutView.LetterboxAmount = PCM->LetterboxAmount;
			OutView.LetterboxAspect = PCM->LetterboxAspect;
			OutView.VignetteIntensity = PCM->VignetteIntensity;

			// 시퀀서 Fade 트랙 / Settings 슬라이더가 PCM 값을 덮어씀
			if (GCameraOverlayDebug.bOverride)
			{
				const FCameraOverlayDebug& D = GCameraOverlayDebug;
				OutView.OverlayColor = FLinearColor{ D.FadeColor[0], D.FadeColor[1], D.FadeColor[2], D.FadeAmount };
				OutView.LetterboxAmount = D.LetterboxAmount;
				OutView.LetterboxAspect = D.LetterboxAspect;
				OutView.VignetteIntensity = D.VignetteIntensity;
			}

			return OutView;
		}
	}

	if (CameraComponent)
	{
		CameraComponent->SetAspectRatio(Aspect);

		OutView.ViewLocation = CameraComponent->GetWorldLocation();
		OutView.ViewRotation = CameraComponent->GetWorldRotation();
		OutView.ViewForward = OutView.ViewRotation.Quaternion().GetForwardVector();
		OutView.ViewMatrix = CameraComponent->GetViewMatrix();
		OutView.ProjectionMatrix = CameraComponent->GetProjectionMatrix();
		OutView.ViewProjectionMatrix = CameraComponent->GetViewProjectionMatrix();
		OutView.FieldOfView = CameraComponent->GetFieldOfView();
		OutView.AspectRatio = Aspect;
		OutView.NearClip = CameraComponent->GetNearZ();
		OutView.FarClip = CameraComponent->GetFarZ();
		OutView.OrthoWidth = CameraComponent->GetOrthoWidth();
		OutView.bIsPerspective = !CameraComponent->GetIsOrthogonal();
		OutView.bIsWireframe = false;
	}
	else
	{
		OutView.ViewLocation = FVector(-5.0f, -10.0f, 10.0f);
		OutView.ViewRotation = FRotator(30.0f, 45.0f, 0.0f);
		OutView.ViewForward = OutView.ViewRotation.Quaternion().GetForwardVector();
		OutView.FieldOfView = 60.0f;
		OutView.AspectRatio = Aspect;
		OutView.NearClip = 0.1f;
		OutView.FarClip = 1000.0f;
		OutView.bIsPerspective = true;
		OutView.bIsWireframe = false;
	}

	OutView.Frustum = ExtractFrustumPlanes(OutView.ViewProjectionMatrix);
	OutView.FrustumCorners = GenerateFrustumCorners(OutView.ViewProjectionMatrix.Inverse());

	if (GCameraOverlayDebug.bOverride)
	{
		const FCameraOverlayDebug& D = GCameraOverlayDebug;
		OutView.OverlayColor = FLinearColor{ D.FadeColor[0], D.FadeColor[1], D.FadeColor[2], D.FadeAmount };
		OutView.LetterboxAmount = D.LetterboxAmount;
		OutView.LetterboxAspect = D.LetterboxAspect;
		OutView.VignetteIntensity = D.VignetteIntensity;
	}


	return OutView;
}

void FGameViewportClient::SetCameraComponent(UCameraComponent* InCameraComponent)
{
	CameraComponent = InCameraComponent;
}

void FGameViewportClient::Draw(FViewport* InViewport, FUIRenderer* Canvas)
{
	FRenderer* Renderer = GEngine ? GEngine->GetEngineLoop().GetRenderer() : nullptr;
	if (!World || !Renderer || !InViewport) return;

	const uint32 Width = static_cast<uint32>(InViewport->GetSizeXY().X);
	const uint32 Height = static_cast<uint32>(InViewport->GetSizeXY().Y);
	if (!Width || !Height) return;

	// 카메라를 뷰·투영 행렬로 바꾼다.
	const FRect ViewRect{ 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height) };
	const FSceneView SceneView = CalcSceneView(ViewRect);

	// 보이는 물체와 라이트를 모은 뒤 패스를 순서대로 실행해 뷰포트의 타깃에 그린다.
	// 뷰 묶음에는 어디에, 무엇을, 어떤 조건으로 그릴지를 담는다. 게임 화면은 항상 평소 화면(VMI_Lit)으로 그린다.
	FSceneViewFamily ViewFamily;
	ViewFamily.RenderTarget = &InViewport->GetViewTargets();
	ViewFamily.World = World;
	ViewFamily.EngineShowFlags = EngineShowFlags;
	ViewFamily.RenderOptions = RenderOptions;
	ViewFamily.Views.Add(&SceneView);

	FSceneRenderer SceneRenderer(ViewFamily, RenderData);
	SceneRenderer.InitViews(Renderer);
	SceneRenderer.Render(Renderer,
		[this, Canvas, Width, Height, &SceneView](FScenePassCallbacks& Callbacks)
		{
			// 게임 UI는 씬 위에 그려야 하므로 그 시점에 불릴 함수로 등록한다.
			Callbacks.Overlays.GameUI = [this, Canvas, Width, Height]
				{
					if (!Canvas) return;
					PaintUI(UIDrawList);
					Canvas->Render(UIDrawList, Width, Height);
				};

			// 에디터가 등록해 둔 디버그 요소가 있으면 같이 등록한다.
			if (DebugDrawDelegate) DebugDrawDelegate(SceneView, Callbacks.Overlays);
		});
}

void FGameViewportClient::Reset()
{
	// PIE가 끝나면 화면에 남은 위젯을 정리한다.
	ClearViewportWidgets();

	RenderData.ResetScene();
	LostFocus();
	bExitRequested = false;
	World = nullptr;
	GameInstance = nullptr;
	Engine = nullptr;
	CameraComponent = nullptr;
}

void FGameViewportClient::AddViewportWidget(UUserWidget* Widget)
{
	if (!Widget || ViewportWidgets.Find(Widget) != INDEX_NONE) return;
	Widget->OwningViewport = this;

	// ZOrder가 같으면 나중에 추가한 것이 위에 온다.
	int32 Index = 0;
	while (Index < ViewportWidgets.Num() && ViewportWidgets[Index]->ZOrder <= Widget->ZOrder)
		++Index;
	ViewportWidgets.Insert(Widget, Index);
}

void FGameViewportClient::RemoveViewportWidget(UUserWidget* Widget)
{
	const int32 Index = ViewportWidgets.Find(Widget);
	if (Index == INDEX_NONE) return;
	ViewportWidgets.RemoveAt(Index);
	Widget->OwningViewport = nullptr;

	// 빠진 트리 안을 가리키고 있을 수 있으니 비운다.
	HoveredWidget = nullptr;
	PressedWidget = nullptr;
}

void FGameViewportClient::ClearViewportWidgets()
{
	const TArray<UUserWidget*> Widgets = ViewportWidgets;
	for (UUserWidget* Widget : Widgets)
		Widget->Destroy();
	ViewportWidgets.Empty();
	HoveredWidget = nullptr;
	PressedWidget = nullptr;
}

void FGameViewportClient::TickUI(float DeltaTime)
{
	// Tick 중에 위젯이 스스로 빠질 수 있어 복사본으로 돈다.
	const TArray<UUserWidget*> Widgets = ViewportWidgets;
	for (UUserWidget* Widget : Widgets)
		Widget->Tick(DeltaTime);
}

void FGameViewportClient::PaintUI(FUIDrawList& DrawList)
{
	DrawList.Reset();
	const uint32 Width = GetWidth();
	const uint32 Height = GetHeight();
	if (Width == 0 || Height == 0) return;

	const FUIGeometry Screen{ FUIRect{ 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height) },
		static_cast<float>(Height) / UIDesignHeight };
	for (UUserWidget* Widget : ViewportWidgets)
	{
		Widget->Arrange(Screen);
		Widget->Paint(DrawList, 1.0f);
	}
}

UWidget* FGameViewportClient::HitTestUI(float X, float Y) const
{
	for (auto It = ViewportWidgets.rbegin(); It != ViewportWidgets.rend(); ++It)
		if (UWidget* Hit = (*It)->HitTest(X, Y))
			return Hit;
	return nullptr;
}

bool FGameViewportClient::HandleUIMouseMove(int32 X, int32 Y)
{
	UIMouseX = X;
	UIMouseY = Y;

	UWidget* Hit = HitTestUI(static_cast<float>(X), static_cast<float>(Y));
	if (Hit != HoveredWidget)
	{
		if (HoveredWidget) HoveredWidget->OnMouseLeave();
		HoveredWidget = Hit;
		if (HoveredWidget) HoveredWidget->OnMouseEnter();
	}
	return false;   // 마우스 이동은 카메라도 써야 하므로 소비하지 않는다
}

bool FGameViewportClient::HandleUIKey(int32 Key, bool bDown)
{
	if (Key != static_cast<int32>(EKeyCode::LButton)) return false;

	const float X = static_cast<float>(UIMouseX);
	const float Y = static_cast<float>(UIMouseY);
	if (bDown)
	{
		UWidget* Hit = HitTestUI(X, Y);
		if (Hit && Hit->OnMouseButtonDown())
		{
			PressedWidget = Hit;
			return true;    // 버튼을 눌렀으면 게임으로 넘기지 않는다
		}
		return false;
	}

	if (!PressedWidget) return false;
	PressedWidget->OnMouseButtonUp(PressedWidget->GetCachedRect().Contains(X, Y));
	PressedWidget = nullptr;
	return true;
}

bool FGameViewportClient::HandleUIAxis(EGameInputAxis AxisKey, float Delta)
{
	return false;   // 스크롤 박스를 만들 때 휠을 여기서 처리한다
}
