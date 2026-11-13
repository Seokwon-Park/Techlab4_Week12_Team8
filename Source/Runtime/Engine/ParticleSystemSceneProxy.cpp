#include "EnginePCH.h"
#include "ParticleSystemSceneProxy.h"
#include "Components/ParticleSystemComponent.h"
#include "Render/RenderCommand.h"
#include "Particles/ParticleModuleRequired.h"

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

bool FParticleSystemSceneProxy::EnsureVertexCapacity(FSpriteDrawSlot& Slot, uint32 ParticleCount)
{
	if (Slot.VertexBuffer && Slot.CapacityParticles >= ParticleCount) return true;

	uint32 NewCapacity = std::max(64u, Slot.CapacityParticles);
	while (NewCapacity < ParticleCount) NewCapacity *= 2;
	NewCapacity = FMath::Min(NewCapacity, MaxParticlesPerEmitter);

	constexpr uint32 Stride = sizeof(FParticleSpriteVertex);
	Slot.VertexBuffer = RenderCommand::CreateDynamicVertexBuffer(NewCapacity * 4 * Stride, Stride);
	const bool bOk = Slot.VertexBuffer && Slot.VertexBuffer->GetBuffer();
	Slot.CapacityParticles = bOk ? NewCapacity : 0;
	return bOk;
}

FIndexBuffer* FParticleSystemSceneProxy::GetSharedQuadIndexBuffer()
{
	static TUniquePtr<FIndexBuffer> Shared;
	if (Shared)return Shared.get();

	TArray<uint32> Indices;
	Indices.Reserve(MaxParticlesPerEmitter * 6);
	for (uint32 Quad = 0; Quad < MaxParticlesPerEmitter; ++Quad)
	{
		const uint32 V = Quad * 4;
		Indices.Add(V + 0);
		Indices.Add(V + 1);
		Indices.Add(V + 2);
		Indices.Add(V + 0);
		Indices.Add(V + 2);
		Indices.Add(V + 3);
	}
	Shared = RenderCommand::CreateStaticIndexBuffer(Indices.GetData(), Indices.Num());

	return Shared.get();
}

void FParticleSystemSceneProxy::DrawSpriteEmitter(const FDynamicSpriteEmitterData& Data, int32 EmitterIndex, FRenderQueue& Queue, const FViewContext& View)
{
	const FDynamicSpriteEmitterReplayDataBase& Src = Data.Source;
	UMaterial* Material = static_cast<UMaterial*>(Src.MaterialInterface);
	const int32 Count = FMath::Min(Src.ActiveParticleCount, static_cast<int32>(MaxParticlesPerEmitter));
	if (!Material || Count <= 0) return;

	FSpriteDrawSlot& Slot = GetSlot(View.ViewIndex, EmitterIndex);
	FIndexBuffer* IndexBuffer = GetSharedQuadIndexBuffer();
	if (!EnsureVertexCapacity(Slot, Count) || !IndexBuffer) return;

	const uint8* ParticleData = Src.DataContainer.ParticleData;
	const uint16* ParticleIndices = Src.DataContainer.ParticleIndices;
	const int32 Stride = Src.ParticleStride;

	const FParticleOrder* Order = nullptr;
	//if (Src.SortMode != PSORTMODE_None)
	//{
	//	SortScratch.Reset();
	//	SortScratch.Reserve(Count);
	//	for (int32 i = 0; i < Count; ++i) SortScratch.Add(FParticleOrder(i, 0.0f));
	//	Data.SortSpriteParticles(Src.SortMode, Src.bUseLocalSpace, Count ,ParticleData, Stride, ParticleIndices, &View,)
	//}

	FParticleSpriteVertex* Dst = static_cast<FParticleSpriteVertex*>(RenderCommand::MapWriteDiscard(Slot.VertexBuffer.get()));
	if (!Dst) return;

	for (int32 i = 0; i < Count; ++i)
	{
		const int32 ActiveIndex = Order ? Order[i].ParticleIndex : i;
		DECLARE_PARTICLE_CONST(Particle, ParticleData + Stride * ParticleIndices[ActiveIndex]);

		FParticleSpriteVertex V;
		V.Position = Particle.Location;
		V.RelativeTime = Particle.RelativeTime;
		V.OldPosition = Particle.OldLocation;
		V.ParticleId = 0.0f;
		V.Size = FVector2(std::abs(Particle.Size.X) * Src.Scale.X, std::abs(Particle.Size.Y) * Src.Scale.Y);
		V.Rotation = Particle.Rotation;
		V.SubImageIndex = 0.0f;
		if (Src.SubUVDataOffset > 0)
		{
			const uint8* Base = reinterpret_cast<const uint8*>(&Particle);
			V.SubImageIndex = *reinterpret_cast<const float*>(Base + Src.SubUVDataOffset);
		}
		V.Color = Particle.Color;

		Dst[i * 4 + 0] = V;
		Dst[i * 4 + 1] = V;
		Dst[i * 4 + 2] = V;
		Dst[i * 4 + 3] = V;
	}
	RenderCommand::Unmap(Slot.VertexBuffer.get());

	FVector Forward = View.CameraForward.Normalized();
	FVector Right = FVector::Cross(FVector(0.0f, 0.0f, 1.0f), Forward).Normalized();
	if (Right.Length() <= 1e-6f) Right = FVector(0.0f, 1.0f, 0.0f);
	const FVector Up = FVector::Cross(Forward, Right).Normalized();

	const bool bAdditive = Material->PSOType == EPSOType::Particle_Additive;
	Slot.Constants.CameraRight = FVector4(Right.X, Right.Y, Right.Z, 0.0f);
	Slot.Constants.CameraUp = FVector4(Up.X, Up.Y, Up.Z, 0.0f);
	Slot.Constants.SubUVInfo = FVector4(static_cast<float>(Src.SubImages_Horizontal), static_cast<float>(Src.SubImages_Vertical), bAdditive ? 1.0f : 0.0f, 0.0f);

	FRenderPacket& Packet = Queue.AddDefaulted_GetRef();
	Packet.Proxy = nullptr;
	Packet.Model = Queue.StoreWorldMatrix(Src.bUseLocalSpace ? GetLocalToWorld() : FMatrix::Identity);
	Packet.VertexBuffer = Slot.VertexBuffer.get();
	Packet.IndexBuffer = IndexBuffer;
	Packet.Material = Material;
	Packet.MaterialParamData = &Slot.Constants;
	Packet.MaterialParamDataSize = sizeof(FParticleSpriteConstants);
	Packet.StartIndex = 0;
	Packet.IndexCount = static_cast<uint32>(Count) * 6;

	const FVector Delta = GetBounds().Center - View.CameraPosition;
	Packet.CameraToParticleDistance = Delta.Dot(Delta);
}
