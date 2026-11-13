#include "EnginePCH.h"
#include <malloc.h>
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"

// [Week12 0단계] UE의 Private/Particles/ParticleEmitterInstances.cpp 에 대응한다.
// 인터페이스만 있다. TODO가 적힌 함수는 다른 담당자가 링크 오류 없이 작업할 수 있게 둔 빈 본문이다.

/*-----------------------------------------------------------------------------
	FParticleDataContainer
	렌더러 담당이 코어 없이 ReplayData를 손으로 채워 테스트할 수 있도록 이것만 구현해 둔다.
-----------------------------------------------------------------------------*/
void FParticleDataContainer::Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts)
{
	Free();

	ParticleDataNumBytes = InParticleDataNumBytes;
	ParticleIndicesNumShorts = InParticleIndicesNumShorts;

	MemBlockSize = ParticleDataNumBytes + ParticleIndicesNumShorts * sizeof(uint16);
	if (MemBlockSize <= 0)
	{
		return;
	}

	ParticleData = (uint8*)_aligned_malloc((size_t)MemBlockSize, PARTICLE_ALIGNMENT);
	ParticleIndices = (uint16*)(ParticleData + ParticleDataNumBytes);
}

void FParticleDataContainer::Free()
{
	if (ParticleData)
	{
		_aligned_free(ParticleData);
	}
	MemBlockSize = 0;
	ParticleDataNumBytes = 0;
	ParticleIndicesNumShorts = 0;
	ParticleData = nullptr;
	ParticleIndices = nullptr;
}

/*-----------------------------------------------------------------------------
	FParticleEmitterInstance
-----------------------------------------------------------------------------*/
FParticleEmitterInstance::FParticleEmitterInstance()
	: SpriteTemplate(nullptr)
	, Component(nullptr)
	, CurrentLODLevel(nullptr)
	, CurrentLODLevelIndex(0)
	, TypeDataOffset(0)
	, TypeDataInstanceOffset(-1)
	, SubUVDataOffset(0)
	, PayloadOffset(0)
	, Location(0.0f, 0.0f, 0.0f)
	, bEnabled(true)
	, bKillOnDeactivate(false)
	, bKillOnCompleted(false)
	, bRequiresSorting(false)
	, bHaltSpawning(false)
	, bEmitterIsDone(false)
	, SortMode(PSORTMODE_None)
	, ParticleData(nullptr)
	, ParticleIndices(nullptr)
	, InstanceData(nullptr)
	, InstancePayloadSize(0)
	, ParticleSize(0)
	, ParticleStride(0)
	, ActiveParticles(0)
	, ParticleCounter(0)
	, MaxActiveParticles(0)
	, SpawnFraction(0.0f)
	, SecondsSinceCreation(0.0f)
	, EmitterTime(0.0f)
	, LastDeltaTime(0.0f)
	, OldLocation(0.0f, 0.0f, 0.0f)
	, LoopCount(0)
	, EmitterDuration(0.0f)
	, CurrentMaterial(nullptr)
{
}

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	// TODO(1번 코어): ParticleData / ParticleIndices / InstanceData 해제
}

void FParticleEmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	// TODO(1번 코어)
}

void FParticleEmitterInstance::Init()
{
	// TODO(1번 코어): ParticleSize를 16의 배수로 올려 ParticleStride를 정하고 메모리를 준비한다.
}

bool FParticleEmitterInstance::Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount)
{
	// TODO(1번 코어)
	return false;
}

void FParticleEmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	// TODO(1번 코어)
}

uint32 FParticleEmitterInstance::GetModuleDataOffset(UParticleModule* Module)
{
	// TODO(1번 코어)
	return 0;
}

float FParticleEmitterInstance::Spawn(float DeltaTime)
{
	// TODO(1번 코어)
	return 0.0f;
}

void FParticleEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload)
{
	// TODO(1번 코어)
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	// TODO(1번 코어)
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime)
{
	// TODO(1번 코어)
}

void FParticleEmitterInstance::KillParticles()
{
	// TODO(1번 코어)
}

void FParticleEmitterInstance::KillParticle(int32 Index)
{
	// TODO(1번 코어): swap-remove
}

bool FParticleEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
	// TODO(3번 렌더 데이터 경계): 공통 필드를 채우고 파티클 메모리와 인덱스 배열을 OutData.DataContainer로 복사한다.
	if (ActiveParticles <= 0 || !bEnabled || !ParticleData || !ParticleIndices)
	{
		return false;
	}

	OutData.eEmitterType = DET_Unknown;
	OutData.ActiveParticleCount = ActiveParticles;
	OutData.ParticleStride = ParticleStride;
	OutData.SortMode = SortMode;
	OutData.Scale = FVector(1.0f);

	const int32 ParticleMemSize = MaxActiveParticles * ParticleStride;
	OutData.DataContainer.Alloc(ParticleMemSize, MaxActiveParticles);
	if (!OutData.DataContainer.ParticleData)
	{
		return false;
	}

	std::memcpy(OutData.DataContainer.ParticleData, ParticleData, ParticleMemSize);
	std::memcpy(OutData.DataContainer.ParticleIndices, ParticleIndices, OutData.DataContainer.ParticleIndicesNumShorts * sizeof(uint16));

	return true;
}

/*-----------------------------------------------------------------------------
	ParticleSpriteEmitterInstance
-----------------------------------------------------------------------------*/
FDynamicEmitterDataBase* FParticleSpriteEmitterInstance::GetDynamicData(bool bSelected)
{
	// TODO(3번 렌더 데이터 경계)
	UParticleLODLevel* LOD = CurrentLODLevel;
	if (!LOD || !LOD->bEnabled || !LOD->RequiredModule || ActiveParticles <= 0)
	{
		return nullptr;
	}

	FDynamicSpriteEmitterData* NewData = new FDynamicSpriteEmitterData(LOD->RequiredModule);
	if (!FillReplayData(NewData->Source))
	{
		delete NewData;
		return nullptr;
	}

	NewData->Init(bSelected);
	return NewData;
}

FDynamicEmitterReplayDataBase* FParticleSpriteEmitterInstance::GetReplayData()
{
	// TODO(3번 렌더 데이터 경계)
	FDynamicSpriteEmitterReplayData* NewData = new FDynamicSpriteEmitterReplayData();
	if (!FillReplayData(*NewData))
	{
		delete NewData;
		return nullptr;
	}
	return NewData;
}

bool FParticleSpriteEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
	// TODO(3번 렌더 데이터 경계)
	UParticleLODLevel* LOD = CurrentLODLevel;
	if (!LOD || !LOD->bEnabled || !LOD->RequiredModule)
	{
		return false;
	}

	// 공통 필드 채우기 및 파티클 할당
	if (!FParticleEmitterInstance::FillReplayData(OutData))
	{
		return false;
	}

	OutData.eEmitterType = DET_Sprite;
	FDynamicSpriteEmitterReplayDataBase& Data = static_cast<FDynamicSpriteEmitterReplayDataBase&>(OutData);

	UParticleModuleRequired* Required = LOD->RequiredModule;
	Data.MaterialInterface = CurrentMaterial ? CurrentMaterial : Required->Material;
	Data.RequiredModule = nullptr;  // 컷아웃·SubUV 확장 때 FParticleRequiredModule 사본을 만든다
	Data.bUseLocalSpace = Required->bUseLocalSpace;
	Data.SubUVDataOffset = SubUVDataOffset;
	Data.SubImages_Horizontal = 1;
	Data.SubImages_Vertical = 1;
	Data.MaxDrawCount = ActiveParticles;
	return true;
}

/*-----------------------------------------------------------------------------
	ParticleMeshEmitterInstance
-----------------------------------------------------------------------------*/
FDynamicEmitterDataBase* FParticleMeshEmitterInstance::GetDynamicData(bool bSelected)
{
	// TODO(4번 Mesh 렌더링)
	return nullptr;
}

FDynamicEmitterReplayDataBase* FParticleMeshEmitterInstance::GetReplayData()
{
	// TODO(4번 Mesh 렌더링)
	return nullptr;
}

bool FParticleMeshEmitterInstance::FillReplayData(FDynamicEmitterReplayDataBase& OutData)
{
	// TODO(4번 Mesh 렌더링)
	return false;
}
