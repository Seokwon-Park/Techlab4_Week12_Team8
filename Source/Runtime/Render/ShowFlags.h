#pragma once

// 뷰포트에 무엇을 보여 줄지 켜고 끄는 스위치 묶음. 품질이나 방식이 아니라 "보이느냐"만 담는다.
// UE의 FEngineShowFlags와 같은 자리이고, UE도 ShowFlags.h에 둔다.
// CameraOverlay를 뺀 이름은 UE의 쇼 플래그 이름과 같다. UE는 비트 필드로 수십 개를 갖고 이 엔진은 쓰는 것만 둔다.
struct FEngineShowFlags
{
	// 물체를 그릴지. 끄면 하늘과 에디터 요소만 남는다.
	bool Rendering = true;
	// 높이 안개
	bool Fog = true;
	// 안티에일리어싱(FXAA)
	bool AntiAliasing = false;
	// 데칼
	bool Decals = true;
	// 화면 가장자리를 어둡게 하는 비네트
	bool Vignette = true;
	// 카메라 페이드와 레터박스. UE에는 없는 이 엔진 고유의 플래그다.
	bool CameraOverlay = true;
	// 스켈레탈 메시
	bool SkeletalMeshes = true;
};

// 뷰포트가 씬을 어떤 방식으로 보여 줄지. 한 번에 하나만 고른다.
// UE의 EViewModeIndex와 같은 자리다. UE는 EngineBaseTypes.h에 둔다.
// VMI_Wireframe, VMI_Lit은 UE에 있는 값이다. 나머지는 이 엔진 고유의 값이다.
// UE는 깊이나 노멀 같은 버퍼 보기를 VMI_VisualizeBuffer 하나와 버퍼 이름으로 고른다.
enum EViewModeIndex
{
	VMI_Wireframe,
	// 평소 화면
	VMI_Lit,
	// 깊이를 밝기로 보여 준다.
	VMI_SceneDepth,
	// 노멀을 색으로 보여 준다.
	VMI_WorldNormal,
	// 화면 칸마다 영향을 주는 광원 수를 색으로 보여 준다. UE의 VMI_LightComplexity에 가깝다.
	VMI_LightCulling,
	// 캐스케이드 그림자의 단계를 색으로 보여 준다.
	VMI_Cascade,
	// 화면 크기 그림자 마스크를 보여 준다.
	VMI_ShadowMask,
};
