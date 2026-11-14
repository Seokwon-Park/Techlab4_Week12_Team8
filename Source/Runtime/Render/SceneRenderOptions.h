#pragma once

#include "Render/RenderEnums.h"
#include "Render/ShowFlags.h"

// 뷰 하나의 씬 렌더링 옵션. 에디터와 게임 뷰포트가 같은 설정을 전달한다.
// 무엇을 보여 줄지(ShowFlags), 어떤 방식으로 보여 줄지(ViewMode), 그리고 품질과 렌더링 방식 설정으로 나뉜다.
// UE는 앞의 둘을 FSceneViewFamily가 갖고(EngineShowFlags, ViewMode), 품질 설정은 콘솔 변수(r.Shadow.MaxResolution 등)로 둔다.
struct FSceneRenderOptions
{
	FEngineShowFlags ShowFlags;
	EViewModeIndex ViewMode = VMI_Lit;

	// ---------- 아래는 품질과 렌더링 방식 설정 ----------
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
