#pragma once

#include "PrimitiveSceneProxy.h"
#include "Particles/ParticleHelper.h"
#include "Render/Buffer.h"

class UParticleSystemComponent;
class UMaterial;

struct FParticleSpriteConstants
{
	FVector4 CameraRight; // xyz: 화면 오른쪽
	FVector4 CameraUp;	// xyz: 화면 위쪽
	FVector4 SubUVInfo; // x : 가로 칸 수, y : 세로 칸 수, z: 1 Additive, w: 미사용
};


class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FParticleSystemSceneProxy(UParticleSystemComponent* InComponent);

	void UpdateRenderState() override;
	bool HasDynamicMeshElements() const override { return true; }
	void GetDynamicMeshElements(FRenderQueue& Queue, const FViewContext& View) override;

	static constexpr uint32 MaxParticlesPerEmitter = 65535;
private:

	struct FSpriteDrawSlot
	{
		TUniquePtr<FVertexBuffer> VertexBuffer;
		uint32 CapacityParticles = 0;   // 쿼드 수 (스프라이트는 파티클 수와 같다)
		uint32 VertexStride = 0;        // 추가: 64(스프라이트) 또는 36(띠). 바뀌면 VB를 다시 만든다
		FParticleSpriteConstants Constants{};
		// 띠 전용 머티리얼 사본. Required가 주는 머티리얼은 Sprite PSO라 띠 정점(36B)을 Sprite 셰이더(64B)로 읽게 된다
		UMaterial* TrailMaterial = nullptr;
		~FSpriteDrawSlot();
	};

	// Source와 같은 텍스처·샘플러로 ParticleTrail_* PSO를 쓰는 머티리얼을 돌려준다
	static UMaterial* GetTrailMaterial(FSpriteDrawSlot& Slot, UMaterial* Source);

	struct FTrailPoint
	{
		// FLinearColor 기본 생성자가 explicit이라 {} 초기화가 되도록 멤버마다 기본값을 준다
		FVector      Position = FVector(0.0f, 0.0f, 0.0f);   // 월드
		float        Width = 0.0f;
		FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		float        U = 0.0f;
	};

	struct FTrailStrip { int32 First; int32 Num; };   // TrailPoints[First .. First+Num) 가 띠 하나


	FSpriteDrawSlot& GetSlot(int32 ViewIndex, int32 EmitterIndex);
	static bool EnsureVertexCapacity(FSpriteDrawSlot& Slot, uint32 QuadCount, uint32 VertexStride);
	static FIndexBuffer* GetSharedQuadIndexBuffer();

	void DrawSpriteEmitter(const FDynamicSpriteEmitterData& Data, int32 EmitterIndex, FRenderQueue& Queue, const FViewContext& View);
	void DrawMeshEmitter(const FDynamicMeshEmitterData& Data, FRenderQueue& Queue, const FViewContext& View);
	void DrawTrailEmitter(const FDynamicSpriteEmitterDataBase& Data, int32 EmitterIndex, FRenderQueue& Queue, const FViewContext& View);

	void BuildBeamPoints(const FDynamicBeamEmitterReplayData& Src);
	void BuildRibbonPoints(const FDynamicRibbonEmitterReplayData& Src);
	FVector ToWorldPoint(bool bLocalSpace, const FVector& P) const
	{
		return bLocalSpace ? GetLocalToWorld().TransformPosition(P) : P;
	}
	static uint32 WriteCameraFacingStrip(const FTrailPoint* Points, int32 Num,
		const FVector& CameraPosition, FParticleTrailVertex* Dst);

	TMap<uint64, TUniquePtr<FSpriteDrawSlot>> Slots;
	TArray<FParticleOrder> SortScratch;
	int16 SortPriority = 0;

	TArray<FTrailPoint>    TrailPoints;   // 프레임마다 재사용 (할당 줄이기)
	TArray<FTrailStrip>    TrailStrips;
	TArray<FParticleOrder> TrailOrder;


};
