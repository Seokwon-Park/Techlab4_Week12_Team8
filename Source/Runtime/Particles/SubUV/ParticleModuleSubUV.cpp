#include "EnginePCH.h"
#include "ParticleModuleSubUV.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"

#include <cmath>

UParticleModuleSubUV::UParticleModuleSubUV()
{
	bSpawnModule = true;
	bUpdateModule = true;
}

// 시트의 칸 수 = Required의 가로 × 세로
int32 UParticleModuleSubUV::GetFrameCount(const FParticleEmitterInstance* Owner)
{
	const UParticleLODLevel* LOD = Owner ? Owner->CurrentLODLevel : nullptr;
	const UParticleModuleRequired* Required = LOD ? LOD->RequiredModule : nullptr;
	if (!Required) return 1;
	return std::max(1, Required->SubImagesHorizontal) * std::max(1, Required->SubImagesVertical);
}

float UParticleModuleSubUV::CalcImageIndex(const FBaseParticle& Particle, float StartFrame, int32 FrameCount) const
{
	const float Count = static_cast<float>(FrameCount);
	float Frame = StartFrame;

	switch (Mode)
	{
	case ESubUVMode::OverLife:
		// RelativeTime 0 → 1 동안 칸 수만큼 진행. 1.0 직전에 마지막 칸이 되도록 Count를 곱한다
		Frame += std::min(Particle.RelativeTime, 0.9999f) * Count;
		break;
	case ESubUVMode::FrameRate:
	{
		// 나이(초) = RelativeTime × 수명. 수명이 0이면(무한) RelativeTime만 쓴다
		const float Age = Particle.OneOverMaxLifetime > 0.0f ? Particle.RelativeTime / Particle.OneOverMaxLifetime : Particle.RelativeTime;
		Frame += Age * std::max(0.0f, FramesPerSecond);
		break;
	}
	case ESubUVMode::Random:
		break;   // Spawn에서 고른 StartFrame 그대로
	}

	// 칸 범위로 감는다 (시작 칸이 랜덤이면 끝을 넘어 처음으로 돌아온다)
	Frame = std::fmod(std::floor(Frame), Count);
	return Frame < 0.0f ? Frame + Count : Frame;
}

void UParticleModuleSubUV::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;
	PARTICLE_ELEMENT(FSubUVPayload, Payload);

	const int32 FrameCount = GetFrameCount(Owner);
	const bool bRandom = bRandomStartFrame || Mode == ESubUVMode::Random;
	Payload.StartFrame = bRandom ? std::floor(FMath::FRandRange(0.0f, static_cast<float>(FrameCount) - 0.001f)) : 0.0f;
	Payload.ImageIndex = CalcImageIndex(Particle, Payload.StartFrame, FrameCount);
}

void UParticleModuleSubUV::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	const int32 FrameCount = GetFrameCount(Owner);
	BEGIN_UPDATE_LOOP;
	PARTICLE_ELEMENT(FSubUVPayload, Payload);
	Payload.ImageIndex = CalcImageIndex(Particle, Payload.StartFrame, FrameCount);
	END_UPDATE_LOOP;
}
