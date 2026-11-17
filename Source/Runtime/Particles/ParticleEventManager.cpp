#include "EnginePCH.h"
#include "ParticleEventManager.h"
#include "Engine/World.h"

void AParticleEventManager::EndPlay()
{
	Super::EndPlay();

	if (UWorld* World = GetWorld())
	{
		if (World->MyParticleEventManager == this)
		{
			World->MyParticleEventManager = nullptr;
		}
	}
}

void AParticleEventManager::HandleParticleCollisionEvents(
	UParticleSystemComponent* Component,
	const TArray<FParticleEventCollideData>& CollisionEvents
)
{
	if (!Component)
		return;

	for (const FParticleEventCollideData& Event : CollisionEvents)
	{
		Component->OnParticleCollide.Broadcast(
			Event.EventName,
			Event.EmitterTime,
			Event.ParticleTime,
			Event.Location,
			Event.Velocity,
			Event.Direction,
			Event.Normal
		);
	}
}