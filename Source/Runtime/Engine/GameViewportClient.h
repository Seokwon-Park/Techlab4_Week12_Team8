#pragma once

#include "Engine/ViewportClient.h"
#include "Engine/Viewport.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"
#include "Math/Matrix.h"
#include "Render/SceneView.h"
#include "Render/SceneRenderData.h"
#include "Render/SceneRenderOptions.h"
#include "UI/UIDrawList.h"

#include <functional>

class UWorld;
class UUserWidget;
class UWidget;
class UGameInstance;
class UEngine;
class UCameraComponent;
class FRenderer;
class FUIRenderer;

struct FWorldContext;
struct FSceneOverlayCallbacks;

// 게임 화면 위에 디버그 요소를 더 그리려는 쪽이 등록하는 함수. 씬 위에 그릴 일을 Overlays에 채운다.
// UE의 FDebugDrawDelegate에 해당한다. UE는 UDebugDrawService에 등록하고 UGameViewportClient::Draw가 부르지만,
// 이 엔진은 뷰포트 클라이언트가 하나를 직접 갖는다.
using FDebugDrawDelegate = std::function<void(const FSceneView&, FSceneOverlayCallbacks&)>;

// 게임 뷰포트 클라이언트 클래스
class FGameViewportClient : public FViewportClient
{
public:
	FGameViewportClient();
	virtual ~FGameViewportClient() override = default;

	// 월드 및 게임 인스턴스 초기화
	void Init(FWorldContext& InWorldContext, UEngine* InEngine);

	virtual UWorld* GetWorld() const override;

	virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y) override;
	virtual bool InputKey(FViewport* Viewport, int32 Key, bool bDown) override;
	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, EGameInputAxis AxisKey, float Delta, float DeltaTime) override;
	bool ConsumeExitRequest() {
		const bool bRequested = bExitRequested;
		bExitRequested = false;
		return bRequested;
	}

	// 게임 카메라 시점 계산
	FSceneView CalcSceneView(const FRect& InViewRect);

	// 이 뷰포트의 월드를 자기 카메라로 InViewport의 타깃에 그린다. FViewport::Draw가 부른다.
	// UE의 UGameViewportClient::Draw(FViewport*, FCanvas*)와 같다.
	// 무엇을 어떻게 보여 줄지는 EngineShowFlags에서 읽는다.
	// Canvas는 게임 UI를 그리는 데 쓴다. nullptr이면 UI를 그리지 않는다.
	void Draw(FViewport* InViewport, FUIRenderer* Canvas) override;

	// 이 클라이언트가 그리는 뷰포트. 그릴 대상(색상, 깊이 텍스처)과 크기는 뷰포트가 갖는다.
	// 뷰포트를 만든 쪽(게임 엔진, 에디터 엔진)이 채운다.
	// UE의 UGameViewportClient::Viewport와 같은 자리이고 UE에서도 public 멤버다.
	FViewport* Viewport = nullptr;

	// 이 뷰포트가 무엇을 어떻게 보여 줄지 정하는 설정. Draw가 이 값으로 씬을 그린다.
	// UE의 UGameViewportClient::EngineShowFlags와 같은 자리이고 UE에서도 public 멤버다.
	// 게임 빌드는 UGameEngine이 게임 설정으로, PIE는 UEditorEngine이 에디터 설정으로 채운다.
	FSceneRenderOptions EngineShowFlags;

	// Draw가 패스를 등록할 때 부른다. 에디터가 PIE 화면에 충돌 도형, 기즈모 같은 것을 올릴 때 쓴다. 게임 빌드에서는 비어 있다.
	FDebugDrawDelegate DebugDrawDelegate;

	const FRect& GetRect() const { return Rect; }
	void SetRect(const FRect& InRect) { Rect = InRect; }
	// 이 클라이언트가 그리는 뷰포트의 크기. 뷰포트가 아직 없으면 0이다.
	uint32 GetWidth() const { return Viewport ? static_cast<uint32>(Viewport->GetSizeXY().X) : 0; }
	uint32 GetHeight() const { return Viewport ? static_cast<uint32>(Viewport->GetSizeXY().Y) : 0; }
	FSceneRenderData& GetRenderData() { return RenderData; }

	void SetGameInstance(UGameInstance* InGameInstance) { GameInstance = InGameInstance; }
	UGameInstance* GetGameInstance() const { return GameInstance; }

	void SetWorld(UWorld* InWorld);

	void SetCameraComponent(UCameraComponent* InCameraComponent);
	// 이 뷰포트가 실제로 그리고 마우스 Ray를 만드는 카메라. 첫 CalcSceneView 전에는 null일 수 있다.
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

	void Reset();

	virtual void LostFocus() override;

	// 화면에 올라간 UI
	void AddViewportWidget(UUserWidget* Widget);
	void RemoveViewportWidget(UUserWidget* Widget);
	void ClearViewportWidgets();
	void TickUI(float DeltaTime);
	// 화면 크기로 배치하고 DrawList를 채운다 (GameUI 오버레이 패스에서 호출)
	void PaintUI(FUIDrawList& DrawList);

	static constexpr float UIDesignHeight = 1080.0f;

	bool ScreenToWorldRay(
		float X, float Y, FRay* OutRay);

protected:
	virtual bool HandleUIKey(int32 Key, bool bDown);
	virtual bool HandleUIAxis(EGameInputAxis AxisKey, float Delta);
	virtual bool HandleUIMouseMove(int32 X, int32 Y);

private:
	UCameraComponent* FindPlayerCamera() const;
	UWidget* HitTestUI(float X, float Y) const;

	TArray<UUserWidget*> ViewportWidgets;   // ZOrder 오름차순
	UWidget* HoveredWidget = nullptr;
	UWidget* PressedWidget = nullptr;
	int32 UIMouseX = 0;
	int32 UIMouseY = 0;

	UWorld* World = nullptr;
	UGameInstance* GameInstance = nullptr;
	UEngine* Engine = nullptr;
	UCameraComponent* CameraComponent = nullptr;
	bool bExitRequested = false;

	// 화면에서 이 뷰포트가 차지하는 영역. 마우스 좌표를 바꿀 때 쓴다.
	FRect Rect{};
	FSceneRenderData RenderData;
	// Draw가 게임 UI를 그릴 때 채우는 목록. 매 프레임 다시 채운다.
	FUIDrawList UIDrawList;
};
