#pragma once

#include "Core/Types.h"
#include "Math/IntPoint.h"
#include "Render/SceneViewTargets.h"

class FViewportClient;

// 화면 한 칸. 그릴 대상(색상, 깊이 텍스처)과 크기를 갖고, 무엇을 그릴지는 뷰포트 클라이언트에게 맡긴다.
// UE의 FViewport와 같은 자리다. UE는 UnrealClient.h에 FViewportClient와 함께 둔다.
// UE의 FViewport는 FRenderTarget을 상속하고 입력(KeyState 등)도 맡지만, 이 엔진은 그리기에 필요한 부분만 둔다.
class FViewport
{
public:
	explicit FViewport(FViewportClient* InViewportClient);

	// 이 뷰포트를 그릴 클라이언트
	FViewportClient* GetClient() const { return ViewportClient; }
	void SetViewportClient(FViewportClient* InViewportClient) { ViewportClient = InViewportClient; }

	// 그릴 대상의 크기(픽셀)
	FIntPoint GetSizeXY() const { return FIntPoint(static_cast<int32>(SizeX), static_cast<int32>(SizeY)); }

	// 그릴 대상을 새 크기로 다시 만든다. bDestroyed가 true면 대상을 버리고 크기를 0으로 만든다.
	// 크기가 바뀌면 클라이언트의 Resize를 불러 알린다.
	// UE의 FViewport::UpdateViewportRHI와 같은 자리다. UE는 창 모드와 픽셀 형식도 인자로 받는다.
	void UpdateViewportRHI(bool bDestroyed, uint32 NewSizeX, uint32 NewSizeY);

	// 클라이언트에게 이 뷰포트를 그리게 한다. 엔진이 뷰포트마다 한 번씩 부른다.
	// UE의 FViewport::Draw와 같은 자리다. UE는 여기서 FCanvas를 만들어 ViewportClient->Draw(this, &Canvas)를 부른다.
	// 이 엔진은 엔진이 갖고 있는 UI 렌더러를 Canvas 자리에 넘긴다.
	// UE의 Draw는 bShouldPresent 인자를 받지만 이 엔진은 Present를 FEngineLoop가 하므로 인자가 없다.
	void Draw();

	// 씬을 그린 결과 색상 텍스처. 아직 만들지 않았으면 nullptr이다.
	// UE의 FRenderTarget::GetRenderTargetTexture와 같은 자리다.
	FTexture2D* GetRenderTargetTexture() const { return ViewTargets.SceneColor.get(); }

	// 색상과 깊이 텍스처 묶음. 씬 렌더러가 이 묶음에 그린다.
	// UE에는 없는 이 엔진 고유의 함수다. UE는 깊이 텍스처를 렌더러가 따로 관리한다.
	FSceneViewTargets& GetViewTargets() { return ViewTargets; }
	const FSceneViewTargets& GetViewTargets() const { return ViewTargets; }

private:
	FViewportClient* ViewportClient = nullptr;
	uint32 SizeX = 0;
	uint32 SizeY = 0;
	FSceneViewTargets ViewTargets;
};
