#include "EnginePCH.h"
#include "ParticleModuleCollision.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Components/ParticleSystemComponent.h"
#include "Engine/World.h"

namespace
{
	inline FVector Reflected(const FVector& V, const FVector& N)
	{
		return V - N * 2.0f * V.Dot(N);
	}
}

UParticleModuleCollision::UParticleModuleCollision()
{
	bSpawnModule = true;
	bUpdateModule = true;
}

void UParticleModuleCollision::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	assert(Offset != 0 && "Collision payload has no offset. Re-Init the emitter instance after changing modules.");

	SPAWN_INIT;
	PARTICLE_ELEMENT(FParticleCollisionPayload, CollisionPayload);

	CollisionPayload.UsedDampingFactor = DampingFactor.GetValue(Owner->EmitterTime);
	CollisionPayload.UsedCollisions = static_cast<int32>(std::lround(MaxCollisions.GetValue(Owner->EmitterTime)));
	CollisionPayload.Delay = DelayAmount.GetValue(Owner->EmitterTime);

	if (CollisionPayload.Delay > 0.0f)
	{
		Particle.Flags |= STATE_Particle_DelayCollisions;
	}
}

void UParticleModuleCollision::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	assert(Offset != 0 && "Collision payload has no offset. Re-Init the emitter instance after changing modules.");

	if (const UParticleSystemComponent* Component = Owner->Component)
	{
		if (UWorld* World = Component->GetWorld())
		{
			BEGIN_UPDATE_LOOP;

			if ((Particle.Flags & STATE_Particle_CollisionIgnoreCheck) != 0)
			{
				CONTINUE_UPDATE_LOOP;
			}

			PARTICLE_ELEMENT(FParticleCollisionPayload, CollisionPayload);

			if ((Particle.Flags & STATE_Particle_DelayCollisions) != 0)
			{
				if (CollisionPayload.Delay > Particle.RelativeTime)
				{
					CONTINUE_UPDATE_LOOP;
				}
				Particle.Flags &= ~STATE_Particle_DelayCollisions;
			}

			FHitResult Result;
			const FVector Start = Particle.Location;
			const FVector Direction = Particle.Velocity.Normalized();
			const float SizeAhead = DirScalar > 0.0f ? (Direction * Particle.Size).Size() / DirScalar : 0.0f;
			const bool bBlocked = World->SweepSingle(
				FRay(Start, Direction),
				Particle.Velocity.Size() * DeltaTime + SizeAhead,
				0.0f, Result, Component->GetOwner()
			);

			if (bBlocked)
			{
				--CollisionPayload.UsedCollisions;

				if (CollisionPayload.UsedCollisions > 0)
				{
					const FVector& Damping = CollisionPayload.UsedDampingFactor;
					Particle.BaseVelocity = Reflected(Particle.BaseVelocity, Result.Normal);
					Particle.Velocity = Reflected(Particle.Velocity, Result.Normal);
					Particle.BaseVelocity *= Damping;
					Particle.Velocity *= Damping;
				}
				else
				{
					Particle.Location = Result.Location;

					switch (CollisionCompletionOption)
					{
					case EParticleCollisionComplete::EPCC_Kill:
						KILL_CURRENT_PARTICLE;
						break;
					case EParticleCollisionComplete::EPCC_Freeze:
						Particle.Flags |= STATE_Particle_Freeze;
						break;
					case EParticleCollisionComplete::EPCC_HaltCollisions:
						Particle.Flags |= STATE_Particle_IgnoreCollisions;
						break;
					case EParticleCollisionComplete::EPCC_FreezeTranslation:
						Particle.Flags |= STATE_Particle_FreezeTranslation;
						break;
					case EParticleCollisionComplete::EPCC_FreezeRotation:
						Particle.Flags |= STATE_Particle_FreezeRotation;
						break;
					case EParticleCollisionComplete::EPCC_FreezeMovement:
						Particle.Flags |= STATE_Particle_FreezeTranslation | STATE_Particle_FreezeRotation;
						break;
					default:
						break;
					}
				}
			}

			END_UPDATE_LOOP;
		}
	}
}

uint32 UParticleModuleCollision::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	return sizeof(FParticleCollisionPayload);
}