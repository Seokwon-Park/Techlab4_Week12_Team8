#include "EnginePCH.h"
#include "Engine/Viewport.h"
#include "Engine/ViewportClient.h"
#include "Engine/Engine.h"

FViewport::FViewport(FViewportClient* InViewportClient)
	: ViewportClient(InViewportClient)
{
}

void FViewport::UpdateViewportRHI(bool bDestroyed, uint32 NewSizeX, uint32 NewSizeY)
{
	if (bDestroyed || NewSizeX == 0 || NewSizeY == 0)
	{
		NewSizeX = 0;
		NewSizeY = 0;
	}

	// 크기가 같고 대상도 멀쩡하면 할 일이 없다. 매 프레임 불러도 된다.
	const bool bHasTargets = NewSizeX > 0 && ViewTargets.IsValidFor(NewSizeX, NewSizeY);
	if (NewSizeX == SizeX && NewSizeY == SizeY && (bHasTargets || NewSizeX == 0))
		return;

	// 클라이언트가 이전 크기(GetSizeXY)와 새 크기를 비교할 수 있도록 크기를 바꾸기 전에 알린다.
	if (ViewportClient && (NewSizeX != SizeX || NewSizeY != SizeY))
		ViewportClient->Resize(this, NewSizeX, NewSizeY);

	SizeX = NewSizeX;
	SizeY = NewSizeY;
	if (SizeX == 0)
		ViewTargets.Reset();
	else
		ViewTargets.Resize(SizeX, SizeY);
}

void FViewport::Draw()
{
	if (!ViewportClient || !GEngine) return;
	// 그릴 대상이 없으면 그리지 않는다. 크기가 0이거나 텍스처 생성에 실패한 경우다.
	if (SizeX == 0 || SizeY == 0 || !ViewTargets.IsValidFor(SizeX, SizeY)) return;

	ViewportClient->Draw(this, GEngine->GetUIRenderer());
}
