#include "EnginePCH.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particles/ParticleEmitterInstances.h"

FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(UParticleEmitter*, UParticleSystemComponent*)
{
	return new FParticleMeshEmitterInstance();
}
