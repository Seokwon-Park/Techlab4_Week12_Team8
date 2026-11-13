#pragma once

#include "PrimitiveSceneProxy.h"
#include "Particles/ParticleHelper.h"
#include "Render/Buffer.h"

class UParticleSystemComponent;

struct FParticleSpriteConstants
{

};


class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FParticleSystemSceneProxy(UParticleSystemComponent* InComponent);

	void UpdateRenderState() override;
	bool HasDynamicMeshElements() const override { return true; }
	void GetDynamicMeshElements(FRenderQueue& QUeue, const FViewContext& View) override;

	static constexpr uint32 MaxParticlesPerEmitter = 65535;
private:

	struct FSpriteDrawSlot
	{
		TUniquePtr<FVertexBuffer> VertexBuffer;
		uint32 CapacityParticles = 0;
		FParticleSpriteConstants Constants{};
	};

	FSpriteDrawSlot& GetSlot(int32 ViewIndex, int32 EmitterIndex);
	static bool EnsureVertexCapacity(FSpriteDrawSlot& Slot, uint32 ParticleCount);
	static FIndexBuffer* GetSharedQuadIndexBuffer();

	void DrawSpriteEmitter(const FDynamicSpriteEmitterData& Data, int32 EmitterIndex, FRenderQueue& Queue, const FViewContext& View);

	TMap<uint64, TUniquePtr<FSpriteDrawSlot>> Slots;
	TArray<FParticleOrder> SortScratch;
	int16 SortPriority = 0;
};