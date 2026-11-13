#include "EnginePCH.h"
#include "ParticleHelper.h"
#include "ParticleModuleRequired.h"
#include "Render/RenderPacket.h"

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
