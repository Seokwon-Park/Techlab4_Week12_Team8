#include "EnginePCH.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleModule.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"

// [Week12 0단계] 인터페이스만 있다. 본문은 1번(런타임 코어) 담당이 채운다.

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODLevel)
{
	// TODO(1번 코어)
	return nullptr;
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	// TODO(1번 코어): ParticleHelper.h의 페이로드 offset 규칙대로 ParticleSize와 ModuleOffsetMap을 계산한다.
}
