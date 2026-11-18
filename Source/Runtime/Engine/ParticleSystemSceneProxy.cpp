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
	SortPriority = static_cast<int16>(std::clamp(GetComponent()->TranslucencySortPriority, -32768, 32767));
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
			DrawMeshEmitter(static_cast<const FDynamicMeshEmitterData&>(*Data), Queue, View);
			break;
		case DET_Beam:
		case DET_Ribbon:
			DrawTrailEmitter(static_cast<const FDynamicSpriteEmitterDataBase&>(*Data), i, Queue, View);
			break;
		default:
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

bool FParticleSystemSceneProxy::EnsureVertexCapacity(FSpriteDrawSlot& Slot, uint32 QuadCount, uint32 VertexStride)
{
	if (Slot.VertexBuffer && Slot.VertexStride == VertexStride && Slot.CapacityParticles >= QuadCount) return true;

	uint32 NewCapacity = (Slot.VertexStride == VertexStride) ? std::max(64u, Slot.CapacityParticles) : 64u;
	while (NewCapacity < QuadCount) NewCapacity *= 2;
	NewCapacity = std::min(NewCapacity, MaxParticlesPerEmitter);

	Slot.VertexBuffer = RenderCommand::CreateDynamicVertexBuffer(NewCapacity * 4 * VertexStride, VertexStride);
	const bool bOk = Slot.VertexBuffer && Slot.VertexBuffer->GetBuffer();
	Slot.CapacityParticles = bOk ? NewCapacity : 0;
	Slot.VertexStride = bOk ? VertexStride : 0;
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
	if (!EnsureVertexCapacity(Slot, Count, sizeof(FParticleSpriteVertex)) || !IndexBuffer) return;

	const uint8* ParticleData = Src.DataContainer.ParticleData;
	const uint16* ParticleIndices = Src.DataContainer.ParticleIndices;
	const int32 Stride = Src.ParticleStride;

	const FParticleOrder* Order = nullptr;
	if (Src.SortMode != PSORTMODE_None)
	{
		SortScratch.Reset();
		SortScratch.Reserve(Count);
		for (int32 i = 0; i < Count; ++i) SortScratch.Add(FParticleOrder(i, 0.0f));
		Data.SortSpriteParticles(Src.SortMode, Src.bUseLocalSpace, Count, ParticleData, Stride,
			ParticleIndices, &View, GetLocalToWorld(), SortScratch.GetData());
		Order = SortScratch.GetData();
	}

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

	const bool bAdditive = Material->PSOType == EPSOType::ParticleSprite_Additive;
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
	Packet.TranslucencySortPriority = SortPriority;
}

void FParticleSystemSceneProxy::DrawMeshEmitter(const FDynamicMeshEmitterData& Data, FRenderQueue& Queue, const FViewContext& View)
{
	UStaticMesh* Mesh = Data.StaticMesh;
	if (!Mesh || !Mesh->GetVertexBuffer(0) || !Mesh->GetIndexBuffer(0)) return;

	const auto& Src = Data.Source;
	const auto& MeshData = Mesh->GetMeshData();
	const int32 Count = FMath::Min(Src.ActiveParticleCount, Src.MaxDrawCount);
	if (Count <= 0) return;

	const FMatrix EmitterToWorld = Src.bUseLocalSpace ? GetLocalToWorld() : FMatrix::Identity;
	const FVector MeshCenter = (MeshData.AABB.Min + MeshData.AABB.Max) * 0.5f;

	TArray<FRenderPacket> InstancedSections;
	TArray<FRenderPacket> IndividualSections;
	for (const FStaticMeshSection& Section : MeshData.Sections)
	{
		UMaterial* Material = Mesh->GetMaterial(Section.MaterialSlotIndex);
		if (!Material || Section.IndexCount == 0) continue;

		// Transparent particles participate individually in the view's back-to-front sort.
		const bool bInstanceSection = Src.bUseGPUInstancing && Material->PSOType == EPSOType::StaticMesh_Opaque;
		FRenderPacket& Packet = (bInstanceSection ? InstancedSections : IndividualSections).AddDefaulted_GetRef();
		Packet.Mesh = Mesh;
		Packet.Material = Material;
		Packet.StartIndex = Section.StartIndex;
		Packet.IndexCount = Section.IndexCount;
		Packet.TranslucencySortPriority = SortPriority;   // 반투명 섹션이 Sprite와 같은 우선순위로 정렬되게 한다
	}
	if (InstancedSections.IsEmpty() && IndividualSections.IsEmpty()) return;

	TArray<FMeshParticleInstanceVertex>* Instances = nullptr;
	if (!InstancedSections.IsEmpty())
	{
		Instances = &Queue.AllocateMeshParticleInstances();
		Instances->Reserve(Count);
	}

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 ParticleIndex = Src.DataContainer.ParticleIndices[Index];
		DECLARE_PARTICLE_CONST(Particle, Src.DataContainer.ParticleData + ParticleIndex * Src.ParticleStride);

		const FMatrix World = BuildMeshParticleTransform(Particle, Src.Scale) * EmitterToWorld;
		if (Instances)
		{
			FMeshParticleInstanceVertex Instance{Particle.Color};
			for (int32 Column = 0; Column < 3; ++Column)
				Instance.Transform[Column] = World.GetColumn(Column);
			Instances->Add(Instance);
		}

		if (IndividualSections.IsEmpty()) continue;
		const FMatrix* Model = Queue.StoreWorldMatrix(World);
		const FVector Delta = World.TransformPosition(MeshCenter) - View.CameraPosition;
		const float DistanceSquared = Delta.Dot(Delta);

		for (const FRenderPacket& Section : IndividualSections)
		{
			Queue.Add(Section);
			FRenderPacket& Packet = Queue.Last();
			Packet.Model = Model;
			Packet.CameraToParticleDistance = DistanceSquared;
		}
	}

	for (FRenderPacket& Section : InstancedSections)
	{
		Section.MeshParticleInstances = { Instances->GetData(), static_cast<size_t>(Instances->Num()) };
		Queue.Add(std::move(Section));
	}
}

uint32 FParticleSystemSceneProxy::WriteCameraFacingStrip(const FTrailPoint* Points, int32 Num, const FVector& CameraPosition, FParticleTrailVertex* Dst)
{
	if (Num < 2) return 0;

	// i번 점에서 띠의 "옆" 방향 × 폭/2. 접선과 카메라 방향 둘 다에 수직이라 띠 면이 카메라를 향한다
	auto SideAt = [&](int32 i)
		{
			const FVector Tangent = Points[std::min(i + 1, Num - 1)].Position - Points[std::max(i - 1, 0)].Position;
			const FVector ToCamera = CameraPosition - Points[i].Position;
			const FVector Side = FVector::Cross(Tangent, ToCamera);
			const float Len = Side.Length();
			return Len > 1e-6f ? Side * (Points[i].Width * 0.5f / Len) : FVector(0.0f, 0.0f, 0.0f);
		};

	uint32 Quads = 0;
	FVector SideA = SideAt(0);
	for (int32 i = 0; i + 1 < Num; ++i)
	{
		const FVector SideB = SideAt(i + 1);
		const FTrailPoint& A = Points[i];
		const FTrailPoint& B = Points[i + 1];

		// Map 받은 메모리는 쓰기만 한다: 지역 변수로 만든 뒤 대입
		FParticleTrailVertex V;
		V.Position = A.Position - SideA; V.UV = FVector2(A.U, 1.0f); V.Color = A.Color; Dst[Quads * 4 + 0] = V;   // 0
		V.Position = A.Position + SideA; V.UV = FVector2(A.U, 0.0f);                    Dst[Quads * 4 + 1] = V;   // 1
		V.Position = B.Position + SideB; V.UV = FVector2(B.U, 0.0f); V.Color = B.Color; Dst[Quads * 4 + 2] = V;   // 2
		V.Position = B.Position - SideB; V.UV = FVector2(B.U, 1.0f);                    Dst[Quads * 4 + 3] = V;   // 3

		SideA = SideB;
		++Quads;
	}
	return Quads;
}

FParticleSystemSceneProxy::FSpriteDrawSlot::~FSpriteDrawSlot()
{
	delete TrailMaterial;
}

UMaterial* FParticleSystemSceneProxy::GetTrailMaterial(FSpriteDrawSlot& Slot, UMaterial* Source)
{
	if (Source->PSOType == EPSOType::ParticleTrail_AlphaBlend || Source->PSOType == EPSOType::ParticleTrail_Additive)
		return Source;   // 이미 띠용 머티리얼

	if (!Slot.TrailMaterial)
	{
		Slot.TrailMaterial = UMaterial::CreateInstance(Source);
		if (!Slot.TrailMaterial) return nullptr;
	}
	// 에디터에서 텍스처·Additive를 바꾸면 Source가 바뀌므로 매 프레임 따라간다
	const bool bAdditive = Source->PSOType == EPSOType::ParticleSprite_Additive || Source->PSOType == EPSOType::Particle_Additive;
	Slot.TrailMaterial->PSOType = bAdditive ? EPSOType::ParticleTrail_Additive : EPSOType::ParticleTrail_AlphaBlend;
	Slot.TrailMaterial->Textures = Source->Textures;
	Slot.TrailMaterial->SamplerState = Source->SamplerState;
	return Slot.TrailMaterial;
}

void FParticleSystemSceneProxy::DrawTrailEmitter(const FDynamicSpriteEmitterDataBase& Data, int32 EmitterIndex, FRenderQueue& Queue, const FViewContext& View)
{
	const FDynamicSpriteEmitterReplayDataBase& Src = *Data.GetSourceData();
	UMaterial* SourceMaterial = static_cast<UMaterial*>(Src.MaterialInterface);
	if (!SourceMaterial) return;

	// 1) 점 만들기 (종류별)
	TrailPoints.Reset();
	TrailStrips.Reset();
	if (Src.eEmitterType == DET_Beam)
		BuildBeamPoints(static_cast<const FDynamicBeamEmitterReplayData&>(Src));
	else
		BuildRibbonPoints(static_cast<const FDynamicRibbonEmitterReplayData&>(Src));

	uint32 TotalQuads = 0;
	for (const FTrailStrip& Strip : TrailStrips) TotalQuads += Strip.Num > 1 ? static_cast<uint32>(Strip.Num - 1) : 0;
	TotalQuads = std::min(TotalQuads, MaxParticlesPerEmitter);   // 공유 IB 크기 한도
	if (TotalQuads == 0) return;

	// 2) VB 채우기
	FSpriteDrawSlot& Slot = GetSlot(View.ViewIndex, EmitterIndex);
	UMaterial* Material = GetTrailMaterial(Slot, SourceMaterial);   // 띠 셰이더를 쓰는 PSO로 바꾼다
	if (!Material) return;
	FIndexBuffer* IndexBuffer = GetSharedQuadIndexBuffer();
	if (!EnsureVertexCapacity(Slot, TotalQuads, sizeof(FParticleTrailVertex)) || !IndexBuffer) return;

	FParticleTrailVertex* Dst = static_cast<FParticleTrailVertex*>(RenderCommand::MapWriteDiscard(Slot.VertexBuffer.get()));
	if (!Dst) return;
	uint32 Written = 0;
	for (const FTrailStrip& Strip : TrailStrips)
	{
		if (Strip.Num < 2 || Written + static_cast<uint32>(Strip.Num - 1) > TotalQuads) continue;
		Written += WriteCameraFacingStrip(&TrailPoints[Strip.First], Strip.Num, View.CameraPosition, Dst + Written * 4);
	}
	RenderCommand::Unmap(Slot.VertexBuffer.get());
	if (Written == 0) return;

	// 3) 상수: 띠 셰이더는 Additive 여부만 본다
	const bool bAdditive = Material->PSOType == EPSOType::ParticleTrail_Additive;
	Slot.Constants.CameraRight = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	Slot.Constants.CameraUp = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	Slot.Constants.SubUVInfo = FVector4(1.0f, 1.0f, bAdditive ? 1.0f : 0.0f, 0.0f);

	// 4) 패킷: 정점이 이미 월드 좌표라 Model은 항상 단위 행렬
	FRenderPacket& Packet = Queue.AddDefaulted_GetRef();
	Packet.Proxy = nullptr;
	Packet.Model = Queue.StoreWorldMatrix(FMatrix::Identity);
	Packet.VertexBuffer = Slot.VertexBuffer.get();
	Packet.IndexBuffer = IndexBuffer;
	Packet.Material = Material;
	Packet.MaterialParamData = &Slot.Constants;
	Packet.MaterialParamDataSize = sizeof(FParticleSpriteConstants);
	Packet.StartIndex = 0;
	Packet.IndexCount = Written * 6;

	const FVector Delta = GetBounds().Center - View.CameraPosition;
	Packet.CameraToParticleDistance = Delta.Dot(Delta);
	Packet.TranslucencySortPriority = SortPriority;
}

void FParticleSystemSceneProxy::BuildRibbonPoints(const FDynamicRibbonEmitterReplayData& Src)
{
	const int32 Count = Src.ActiveParticleCount;
	const uint8* ParticleData = Src.DataContainer.ParticleData;
	const uint16* ParticleIndices = Src.DataContainer.ParticleIndices;

	// 1) 스폰 순번(작을수록 오래됨 = 꼬리)으로 정렬
	TrailOrder.Reset();
	TrailOrder.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		DECLARE_PARTICLE_CONST(P, ParticleData + Src.ParticleStride * ParticleIndices[i]);
		TrailOrder.Add(FParticleOrder(i, static_cast<uint32>(P.Flags & STATE_CounterMask)));
	}
	std::sort(TrailOrder.GetData(), TrailOrder.GetData() + TrailOrder.Num(),
		[](const FParticleOrder& A, const FParticleOrder& B) { return A.C < B.C; });

	// 2) 꼬리 → 머리 순서로 점 추가. U에는 일단 누적 거리를 넣는다
	const int32 First = TrailPoints.Num();
	float Distance = 0.0f;
	FVector Prev;
	FTrailPoint Last{};
	for (int32 n = 0; n < Count; ++n)
	{
		DECLARE_PARTICLE_CONST(P, ParticleData + Src.ParticleStride * ParticleIndices[TrailOrder[n].ParticleIndex]);
		const FVector Pos = ToWorldPoint(Src.bUseLocalSpace, P.Location);
		if (n > 0) Distance += (Pos - Prev).Length();
		Prev = Pos;

		Last.Position = Pos;
		Last.Width = std::abs(P.Size.X) * Src.Scale.X;   // Size 모듈의 X가 띠 폭
		Last.Color = P.Color;                            // ColorOverLife로 꼬리를 투명하게
		Last.U = Distance;
		TrailPoints.Add(Last);
	}

	// 3) 머리를 지금 이미터 위치까지 잇는다 (다음 스폰 전까지 띠가 이미터에서 떨어져 보이지 않게)
	if (Src.bConnectToSource && Count > 0)
	{
		Distance += (Src.SourceLocation - Prev).Length();
		Last.Position = Src.SourceLocation;
		Last.U = Distance;
		TrailPoints.Add(Last);
	}

	// 4) U 정규화: TilingDistance마다 1번 반복, 0이면 띠 전체가 0..1
	const int32 Num = TrailPoints.Num() - First;
	const float Scale = Src.TilingDistance > 0.0f ? 1.0f / Src.TilingDistance
		: (Distance > 0.0f ? 1.0f / Distance : 0.0f);
	for (int32 j = 0; j < Num; ++j) TrailPoints[First + j].U *= Scale;

	TrailStrips.Add({ First, Num });
}


void FParticleSystemSceneProxy::BuildBeamPoints(const FDynamicBeamEmitterReplayData& Src)
{
	const int32 Segments = std::clamp(Src.Segments, 1, 128);
	constexpr float Pi = 3.14159265f;

	for (int32 i = 0; i < Src.ActiveParticleCount; ++i)
	{
		DECLARE_PARTICLE_CONST(P, Src.DataContainer.ParticleData + Src.ParticleStride * Src.DataContainer.ParticleIndices[i]);

		// 두 점 모두 같은 공간에서 더한 뒤 월드로 옮긴다 (로컬 공간이면 TargetOffset도 컴포넌트와 함께 돈다)
		const FVector Start = ToWorldPoint(Src.bUseLocalSpace, P.Location);
		const FVector Target = ToWorldPoint(Src.bUseLocalSpace, P.Location + Src.TargetOffset);
		FVector Dir = Target - Start;
		if (Dir.Length() < 1e-4f) continue;
		Dir = Dir.Normalized();

		// 빔에 수직인 두 축. 노이즈를 이 평면 안에서만 흔든다
		FVector AxisA = FVector::Cross(Dir, FVector(0.0f, 0.0f, 1.0f));
		if (AxisA.Length() < 1e-4f) AxisA = FVector::Cross(Dir, FVector(0.0f, 1.0f, 0.0f));   // 빔이 수직일 때
		AxisA = AxisA.Normalized();
		const FVector AxisB = FVector::Cross(Dir, AxisA);

		const float Phase = static_cast<float>(P.Flags & STATE_CounterMask) * 1.37f;   // 빔마다 다른 모양
		const float Width = std::abs(P.Size.X) * Src.Scale.X;

		// Lightning: 정수 해시로 만든 -1 ~ 1 난수. 같은 (빔, 시간 칸, 마디)면 같은 값 → 한 칸 동안 모양이 고정된다
		const uint32 BeamSeed = (P.Flags & STATE_CounterMask) * 92821u
			+ static_cast<uint32>(std::max(0.0f, Src.EmitterTime * std::max(Src.NoiseRefreshRate, 0.0f))) * 68917u;
		auto Rand11 = [BeamSeed](uint32 Index, uint32 Axis)
			{
				uint32 X = BeamSeed ^ (Index * 0x9E3779B1u) ^ (Axis * 0x85EBCA77u);
				X ^= X >> 16; X *= 0x7FEB352Du; X ^= X >> 15; X *= 0x846CA68Bu; X ^= X >> 16;
				return static_cast<float>(X & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu) * 2.0f - 1.0f;
			};
		// 굵은 꺾임(4마디마다 한 번 뽑아 사이를 잇는다) + 잔떨림(마디마다). 진짜 번개처럼 큰 지그재그 위에 작은 지그재그
		auto LightningOffset = [&](int32 k, uint32 Axis)
			{
				constexpr int32 CoarseEvery = 4;
				const int32 C0 = k / CoarseEvery;
				const float F = static_cast<float>(k % CoarseEvery) / CoarseEvery;
				const float Coarse = Rand11(static_cast<uint32>(C0), Axis) * (1.0f - F) + Rand11(static_cast<uint32>(C0 + 1), Axis) * F;
				const float Fine = Rand11(static_cast<uint32>(k) + 1000u, Axis) * 0.35f;
				return Coarse + Fine;
			};

		const int32 First = TrailPoints.Num();
		for (int32 k = 0; k <= Segments; ++k)
		{
			const float T = static_cast<float>(k) / Segments;                // 0 = 시작점, 1 = 목표점
			const float Envelope = std::sin(T * Pi);                          // 양 끝은 0 → 시작·목표점은 고정
			FVector Noise;
			if (Src.NoiseType == 1)                                           // EBeamNoiseType::Lightning
			{
				Noise = (AxisA * LightningOffset(k, 0) + AxisB * LightningOffset(k, 1)) * (Src.NoiseAmplitude * Envelope);
			}
			else
			{
				const float W = T * Src.NoiseFrequency * 2.0f * Pi + Src.EmitterTime * Src.NoiseSpeed + Phase;
				Noise = (AxisA * std::sin(W) + AxisB * std::cos(W * 1.3f + Phase)) * (Src.NoiseAmplitude * Envelope);
			}

			FTrailPoint Point;
			Point.Position = Start + (Target - Start) * T + Noise;
			Point.Width = Width;
			Point.Color = P.Color;                   // 수명에 따른 페이드는 ColorOverLife
			Point.U = T * Src.TextureTile;
			TrailPoints.Add(Point);
		}
		TrailStrips.Add({ First, Segments + 1 });
	}
}