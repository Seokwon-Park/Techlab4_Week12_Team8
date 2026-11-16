#pragma once

#include "Render/RenderEnums.h"

// 씬을 그릴 때의 품질과 렌더링 방식 설정. 에디터와 게임 뷰포트가 같은 구조체로 전달한다.
// 무엇을 보여 줄지(FEngineShowFlags)와 어떤 방식으로 보여 줄지(EViewModeIndex)는 여기에 없고 FSceneViewFamily가 따로 갖는다.
// UE에는 없는 이 엔진 고유의 구조체다. UE는 이런 값을 콘솔 변수(r.Shadow.MaxResolution 등)로 둔다.
struct FSceneRenderOptions
{
	EPcfMode PcfMode = EPcfMode::Sample8;
	float ShadowDepthBias = 0.0001f;
	bool bCacheShadows = true; // r.Shadow.CacheWholeSceneShadows: 바뀌지 않은 그림자 타일은 다시 그리지 않는다
	ELightCullingMode LightCullingMode = ELightCullingMode::Tiled;
	bool bDecalCulling = true;
	bool bDecalNarrowCulling = true;
	bool bDecalBVHCulling = true;
	bool bDecalOBBTree = true;
	float SceneDepthRange = 1.0f;
	uint32 DirectionalShadowMapSize = 0;
	uint32 PointShadowMapSize = 0;
	uint32 SpotShadowMapSize = 0;

	bool bGammaCorrection = true;
	float DisplayGamma = 2.2f;

	ERenderingPath RenderingPath = ERenderingPath::Deferred;
	ELightingModel LightingModel = ELightingModel::Gouraud;
};
