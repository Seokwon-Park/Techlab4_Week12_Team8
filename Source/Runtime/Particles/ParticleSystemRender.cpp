#include "EnginePCH.h"
#include "Particles/ParticleHelper.h"
#include "Render/RenderPacket.h"
#include <algorithm>

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

FMatrix BuildMeshParticleTransform(const FBaseParticle& Particle, const FVector& Scale)
{
	const FQuat Rotation = FQuat::MakeFromAxisAngle(FVector(0.0f, 0.0f, 1.0f), Particle.Rotation);
	return FTransform(Particle.Location, Particle.Size * Scale, Rotation).GetLocalMatrix();
}
