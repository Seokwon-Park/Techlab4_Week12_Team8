#include "EnginePCH.h"
#include "ParticleSystemSceneProxy.h"
#include "Components/ParticleSystemComponent.h"

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
}

void FParticleSystemSceneProxy::UpdateRenderState()
{
	RenderState.reset();
	UpdateCommonFlags();
}

void FParticleSystemSceneProxy::GetDynamicMeshElements(FRenderQueue& Queue, const FViewContext& View)
{
	const UParticleSystemComponent* PSC = static_cast<const UParticleSystemComponent*>(GetComponent());
	if (!PSC) return;

	const TArray<FDynamicEmitterDataBase*>& RenderData = PSC->EmitterRenderData;
	for (int32 i = 0; i < RenderData.Num(); ++i)
	{
		const FDynamicEmitterDataBase* Data = RenderData[i];
		if (!Data || !Data->bValid) continue;

		switch (Data->GetSource().eEmitterType)
		{
		case DET_Sprite:
			DrawSpriteEmitter(static_cast<const FDynamicSpriteEmitterData&>(*Data), i, Queue, View);
			break;
		case DET_Mesh:
			break;
		cefault:
			break;
		}
	}
}

FParticleSystemSceneProxy::FSpriteDrawSlot& FParticleSystemSceneProxy::GetSlot(int32 ViewIndex, int32 EmitterIndex)
{
	const uint64 Key = (static_cast<uint64>(static_cast<uint32>(ViewIndex)) << 32) | static_cast<uint32>(EmitterIndex);
	TUniquePtr<FSpriteDrawSlot>& Slot = Slots.FindOrAdd(Key);   // 엔진 TMap: 없으면 추가 (operator[]는 없으면 assert)
	if (!Slot) Slot = MakeUnique<FSpriteDrawSlot>();
	return *Slot;
}
