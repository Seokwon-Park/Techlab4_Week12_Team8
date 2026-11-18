#include "EnginePCH.h"
#include "ParticleModuleTypeDataBeam.h"
#include "ParticleModuleTypeDataRibbon.h"
#include "Particles/ParticleEmitterInstances.h"

// PSC::InitParticles가 이 함수로 인스턴스를 받은 뒤 InitParameters → Init을 부른다
FParticleEmitterInstance* UParticleModuleTypeDataBeam::CreateInstance(UParticleEmitter*, UParticleSystemComponent*)
{
	return new FParticleBeamEmitterInstance();
}

FParticleEmitterInstance* UParticleModuleTypeDataRibbon::CreateInstance(UParticleEmitter*, UParticleSystemComponent*)
{
	return new FParticleRibbonEmitterInstance();
}