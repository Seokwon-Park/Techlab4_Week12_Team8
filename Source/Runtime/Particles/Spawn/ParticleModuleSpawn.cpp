#include "EnginePCH.h"
#include "ParticleModuleSpawn.h"
#include "Particles/ParticleEmitterInstances.h"

namespace
{
	int32 RollBurstCount(const FParticleBurst& Burst)
	{
		if (Burst.CountLow < 0)
			return Burst.Count;

		const int32 Low = FMath::Min(Burst.CountLow, Burst.Count);
		const int32 High = FMath::Max(Burst.CountLow, Burst.Count);
		return FMath::RandRange(Low, High);
	}
}

int32 UParticleModuleSpawn::GetBurstCount(float EmitterTime, FParticleBurstFired& InOutFired) const
{
	const FParticleBurst* const Bursts[] = { &Burst0, &Burst1, &Burst2, &Burst3 };
	static_assert(sizeof(Bursts) / sizeof(Bursts[0]) == MaxBursts, "Burst 슬롯 수와 MaxBursts 불일치");

	int32 BurstCount = 0;
	for (int32 Index = 0; Index < MaxBursts; ++Index)
	{
		const FParticleBurst& Burst = *Bursts[Index];
		if (InOutFired.Fired[Index] || Burst.Count <= 0 || EmitterTime < Burst.Time)
		{
			continue;
		}

		InOutFired.Fired[Index] = true;
		BurstCount += RollBurstCount(Burst);
	}
	return BurstCount;
}
