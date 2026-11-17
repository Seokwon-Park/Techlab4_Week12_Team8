#include "EnginePCH.h"
#include "ParticleModuleEventGenerator.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/Collision/ParticleModuleCollision.h"
#include "Components/ParticleSystemComponent.h"
#include "Collision/HitResult.h"

bool UParticleModuleEventGenerator::HandleParticleCollision(
	FParticleEmitterInstance* Owner,
	const FParticleCollisionPayload& CollidePayload,
	const FHitResult& HitResult,
	const FBaseParticle& CollideParticle,
	const FVector& CollideDirection
) const
{
	const FParticleEvent_GenerateInfo& Info = Event0;
	if (Info.Type != EParticleEventType::EPET_Collision || !Owner || !Owner->Component)
	{
		return false;
	}

	if (Info.FirstTimeOnly)
	{
		// 첫 충돌만. 이미 충돌한 적이 있으면 알리지 않음
		if ((CollideParticle.Flags & STATE_Particle_CollisionHasOccurred) != 0)
		{
			return false;
		}
	}
	else if (Info.LastTimeOnly) // UE처럼 둘 다 true면 FirstTimeOnly만 확인함
	{
		// 마지막 충돌만. 아직 충돌할 횟수가 남아있으면 알리지 않음
		if (CollidePayload.UsedCollisions > 0)
		{
			return false;
		}
	}

	Owner->Component->ReportEventCollision(
		Info.CustomName,
		Owner->EmitterTime,
		HitResult.Location,
		CollideDirection,
		CollideParticle.Velocity,
		CollideParticle.RelativeTime,
		HitResult.Normal,
		HitResult.Time
	);

	return true;
}