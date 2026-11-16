#pragma once

#include "Core/Types.h"
#include "Input/InputSystem.h"

class FViewport;
class FUIRenderer;
class UWorld;

class FViewportClient
{
public:
	virtual ~FViewportClient() = default;
	virtual UWorld* GetWorld() const { return nullptr; }
	// Viewport의 그릴 대상에 자기 월드를 그린다. FViewport::Draw가 부른다.
	// UE의 FViewportClient::Draw(FViewport*, FCanvas*)와 같은 자리다.
	// Canvas는 씬 위에 2D(게임 UI)를 그리는 도구다. UE의 FCanvas에 해당하고 이 엔진에서는 FUIRenderer다.
	virtual void Draw(FViewport* Viewport, FUIRenderer* Canvas) {}
	virtual void MouseMove(FViewport*, int32, int32) {}
	virtual bool InputKey(FViewport*, int32, bool) { return false; }
	virtual bool InputAxis(FViewport*, int32, EGameInputAxis, float, float) { return false; }
	virtual void ReceivedFocus() {}
	virtual void LostFocus() {}
	virtual void Resize(FViewport*, uint32, uint32) {}
};
