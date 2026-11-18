#include "EnginePCH.h"
#include "ParticleModuleLight.h"
#include "Particles/ParticleEmitterInstances.h"

UParticleModuleLight::UParticleModuleLight()
{
	bSpawnModule = true;
	bUpdateModule = true;
}

void UParticleModuleLight::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	SPAWN_INIT;
	PARTICLE_ELEMENT(FLightParticlePayload, Payload);

	// 색과 반경은 태어날 때 한 번 뽑아 둔다 (Uniform이면 파티클마다 다른 빛)
	Payload.ColorScale = ColorScale.GetValue(Owner->EmitterTime);
	Payload.Radius = std::max(0.0f, Radius.GetValue(Owner->EmitterTime));
	Payload.FalloffExponent = FalloffExponent;
	Payload.bUseParticleColor = bUseParticleColor ? 1u : 0u;
	Payload.bValid = FMath::FRandRange(0.0f, 1.0f) < SpawnFraction ? 1u : 0u;
	Payload.Intensity = Intensity * BrightnessOverLife.GetValue(Particle.RelativeTime);
}

void UParticleModuleLight::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	BEGIN_UPDATE_LOOP;
	PARTICLE_ELEMENT(FLightParticlePayload, Payload);
	Payload.Intensity = std::max(0.0f, Intensity * BrightnessOverLife.GetValue(Particle.RelativeTime));
	END_UPDATE_LOOP;
}
