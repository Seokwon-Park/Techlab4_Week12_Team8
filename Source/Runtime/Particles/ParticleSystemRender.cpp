#include "EnginePCH.h"
#include "ParticleModuleRequired.h"
#include "Particles/ParticleHelper.h"
#include "Render/RenderPacket.h"
#include <algorithm>

void FDynamicSpriteEmitterDataBase::SortSpriteParticles(int32 SortMode, bool bLocalSpace, int32 ParticleCount, const uint8* ParticleData, int32 ParticleStride, const uint16* ParticleIndices, const FViewContext* View, const FMatrix& LocalToWorld, FParticleOrder* ParticleOrder) const
{
	if (!View || ParticleCount <= 0 || SortMode == PSORTMODE_None) return;

	auto ToWorld = [&](const FBaseParticle& P)
		{
			return bLocalSpace ? LocalToWorld.TransformPosition(P.Location) : P.Location;
		};
	auto FarFirst = [](const FParticleOrder& A, const FParticleOrder& B) { return A.Z > B.Z; };

	switch (SortMode)
	{
	case PSORTMODE_ViewProjDepth:
	{
		const FMatrix& VP = View->ViewProjection;
		for (int32 i = 0; i < ParticleCount; ++i)
		{
			DECLARE_PARTICLE_CONST(P, ParticleData + ParticleStride * ParticleIndices[i]);
			const FVector W = ToWorld(P);
			// 행벡터 × 행렬이므로 clip.w = 위치 · 4번째 열
			const float ClipW = W.X * VP.M[0][3] + W.Y * VP.M[1][3] + W.Z * VP.M[2][3] + VP.M[3][3];
			ParticleOrder[i] = FParticleOrder(i, ClipW);
		}
		std::sort(ParticleOrder, ParticleOrder + ParticleCount, FarFirst);
		break;
	}
	case PSORTMODE_DistanceToView:
	{
		for (int32 i = 0; i < ParticleCount; ++i)
		{
			DECLARE_PARTICLE_CONST(P, ParticleData + ParticleStride * ParticleIndices[i]);
			ParticleOrder[i] = FParticleOrder(i, (ToWorld(P) - View->CameraPosition).SizeSquared());
		}
		std::sort(ParticleOrder, ParticleOrder + ParticleCount, FarFirst);
		break;
	}
	case PSORTMODE_Age_OldestFirst:
	case PSORTMODE_Age_NewestFirst:
	{
		for (int32 i = 0; i < ParticleCount; ++i)
		{
			DECLARE_PARTICLE_CONST(P, ParticleData + ParticleStride * ParticleIndices[i]);
			ParticleOrder[i] = FParticleOrder(i, static_cast<uint32>(P.Flags & STATE_CounterMask));
		}
		if (SortMode == PSORTMODE_Age_OldestFirst)
			std::sort(ParticleOrder, ParticleOrder + ParticleCount,
				[](const FParticleOrder& A, const FParticleOrder& B) { return A.C < B.C; });
		else
			std::sort(ParticleOrder, ParticleOrder + ParticleCount,
				[](const FParticleOrder& A, const FParticleOrder& B) { return A.C > B.C; });
		break;
	}
	default:
		break;
	}
}


void FDynamicSpriteEmitterData::Init(bool bInSelected)
{
	bSelected = bInSelected;
	bValid = Source.ActiveParticleCount > 0
		&& Source.DataContainer.ParticleData != nullptr
		&& Source.MaterialInterface != nullptr;
}

void FDynamicMeshEmitterData::Init(bool bInSelected, const FParticleMeshEmitterInstance*, UStaticMesh* InStaticMesh)
{
	bSelected = bInSelected;
	StaticMesh = InStaticMesh;
	bValid = StaticMesh && Source.ActiveParticleCount > 0 &&
		Source.DataContainer.ParticleData && Source.DataContainer.ParticleIndices;
}

void FDynamicBeamEmitterData::Init(bool bInSelected)
{
	bSelected = bInSelected;
	bValid = Source.ActiveParticleCount > 0 && Source.DataContainer.ParticleData && Source.MaterialInterface;
}

void FDynamicRibbonEmitterData::Init(bool bInSelected)
{
	bSelected = bInSelected;
	// 점 1개로는 띠가 안 되지만, bConnectToSource면 이미터 위치가 두 번째 점이 된다
	const int32 MinPoints = Source.bConnectToSource ? 1 : 2;
	bValid = Source.ActiveParticleCount >= MinPoints && Source.DataContainer.ParticleData && Source.MaterialInterface;
}

FMatrix BuildMeshParticleTransform(const FBaseParticle& Particle, const FVector& Scale)
{
	const FQuat Rotation = FQuat::MakeFromAxisAngle(FVector(0.0f, 0.0f, 1.0f), Particle.Rotation);
	return FTransform(Particle.Location, Particle.Size * Scale, Rotation).GetLocalMatrix();
}
