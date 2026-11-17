#include "EnginePCH.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <malloc.h>
#include <new>
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"

// [Week12 런타임 코어] 템플릿의 설정을 실행하며 파티클 풀과 모듈별 데이터를 소유한다.
// 렌더러에는 FillReplayData()로 복사한 스냅샷을 전달한다.

namespace
{
	constexpr int32 MaxParticleCapacity = std::numeric_limits<uint16>::max();
}

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
	// 1. 이 인스턴스가 소유한 파티클 풀과 인스턴스 페이로드를 해제한다.
	// 2. 정렬 할당은 _aligned_free, 인덱스의 new[]는 delete[]와 짝을 맞춘다.
	// 3. 템플릿, 컴포넌트, 모듈, 머티리얼은 소유하지 않으므로 삭제하지 않는다.
	_aligned_free(ParticleData);
	delete[] ParticleIndices;
	_aligned_free(InstanceData);
}

void FParticleEmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	// 1. 공유 설정인 이미터 템플릿과 실행 소유자인 컴포넌트를 연결한다.
	// 2. 기본 실행 설정으로 LOD 0과 컴포넌트의 월드 위치를 선택한다.
	// 3. 메모리 할당과 시뮬레이션 초기화는 뒤이어 호출할 Init()에 맡긴다.
	SpriteTemplate = InTemplate;
	Component = InComponent;
	CurrentLODLevelIndex = 0;
	CurrentLODLevel = SpriteTemplate ? SpriteTemplate->GetLODLevel(0) : nullptr;
	Location = Component ? Component->GetWorldLocation() : FVector(0.0f);
	OldLocation = Location;
}

void FParticleEmitterInstance::Init()
{
	// 1. 이전 실행 메모리와 시간을 초기화하고 템플릿의 모듈 배치를 캐시한다.
	// 2. 실제 크기 ParticleSize를 16바이트로 올려 ParticleStride를 계산한다.
	// 3. 파티클 풀, 빈 슬롯 인덱스, 이미터당 InstanceData를 준비한다.
	_aligned_free(ParticleData);
	delete[] ParticleIndices;
	_aligned_free(InstanceData);
	ParticleData = nullptr;
	ParticleIndices = nullptr;
	InstanceData = nullptr;
	ActiveParticles = MaxActiveParticles = 0;
	ParticleCounter = 0;
	ParticleSize = ParticleStride = InstancePayloadSize = 0;
	PayloadOffset = TypeDataOffset = SubUVDataOffset = 0;
	TypeDataInstanceOffset = -1;
	SpawnFraction = SecondsSinceCreation = EmitterTime = LastDeltaTime = 0.0f;
	LoopCount = 0;
	EmitterDuration = 0.0f; // Required 모듈에 duration 설정이 없으므로 기본은 연속 실행이다.
	bHaltSpawning = bEmitterIsDone = false;
	Location = Component ? Component->GetWorldLocation() : FVector(0.0f);
	OldLocation = Location;
	CurrentMaterial = nullptr;
	SortMode = PSORTMODE_None;
	bRequiresSorting = false;
	CurrentLODLevelIndex = 0; // 페이로드 캐시는 LOD 0의 배치를 사용한다.
	CurrentLODLevel = SpriteTemplate ? SpriteTemplate->GetLODLevel(0) : nullptr;
	bEnabled = CurrentLODLevel && CurrentLODLevel->bEnabled && CurrentLODLevel->RequiredModule;
	if (!bEnabled)
	{
		return;
	}

	SpriteTemplate->CacheEmitterModuleInfo();
	CurrentLODLevel->UpdateModuleLists();
	ParticleSize = SpriteTemplate->ParticleSize;
	InstancePayloadSize = SpriteTemplate->ReqInstanceBytes;
	if (ParticleSize < static_cast<int32>(sizeof(FBaseParticle)) ||
		ParticleSize > std::numeric_limits<int32>::max() - (PARTICLE_ALIGNMENT - 1) ||
		InstancePayloadSize < 0)
	{
		bEnabled = false;
		return;
	}
	ParticleStride = AlignParticleBytes(ParticleSize);
	PayloadOffset = sizeof(FBaseParticle);
	TypeDataOffset = SpriteTemplate->TypeDataOffset;
	TypeDataInstanceOffset = SpriteTemplate->TypeDataInstanceOffset;
	UParticleModuleRequired* Required = CurrentLODLevel->RequiredModule;
	CurrentMaterial = Required->Material;
	SortMode = Required->SortMode;
	bRequiresSorting = SortMode != PSORTMODE_None;

	if (InstancePayloadSize > 0)
	{
		InstanceData = static_cast<uint8*>(_aligned_malloc(InstancePayloadSize, PARTICLE_ALIGNMENT));
		if (!InstanceData)
		{
			bEnabled = false;
			return;
		}
		std::memset(InstanceData, 0, InstancePayloadSize);
	}

	const int32 InitialCount = SpriteTemplate->InitialAllocationCount > 0
		? SpriteTemplate->InitialAllocationCount : std::max(16, SpriteTemplate->PeakActiveParticles);
	bEnabled = Resize(std::min(InitialCount, MaxParticleCapacity));
}

bool FParticleEmitterInstance::Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount)
{
	// 1. 요구한 수용량이 이미 있으면 유지하고, 부족할 때만 풀을 확장한다.
	// 2. 파티클 본문과 활성/비활성 인덱스 순서를 보존하고 새 빈 슬롯을 붙인다.
	// 3. 새 할당이 모두 성공한 뒤 교체하여 실패 시 기존 파티클을 보존한다.
	if (NewMaxActiveParticles < 0 || NewMaxActiveParticles > MaxParticleCapacity ||
		ParticleStride <= 0 || NewMaxActiveParticles > std::numeric_limits<int32>::max() / ParticleStride)
	{
		return false;
	}
	if (NewMaxActiveParticles > MaxActiveParticles)
	{
		const size_t NewDataBytes = static_cast<size_t>(NewMaxActiveParticles) * ParticleStride;
		uint8* NewData = static_cast<uint8*>(_aligned_malloc(NewDataBytes, PARTICLE_ALIGNMENT));
		uint16* NewIndices = new (std::nothrow) uint16[NewMaxActiveParticles];
		if (!NewData || !NewIndices)
		{
			_aligned_free(NewData);
			delete[] NewIndices;
			return false;
		}
		std::memset(NewData, 0, NewDataBytes);
		if (MaxActiveParticles > 0)
		{
			std::memcpy(NewData, ParticleData, static_cast<size_t>(MaxActiveParticles) * ParticleStride);
			std::memcpy(NewIndices, ParticleIndices, static_cast<size_t>(MaxActiveParticles) * sizeof(uint16));
		}
		for (int32 Index = MaxActiveParticles; Index < NewMaxActiveParticles; ++Index)
		{
			NewIndices[Index] = static_cast<uint16>(Index);
		}
		_aligned_free(ParticleData);
		delete[] ParticleIndices;
		ParticleData = NewData;
		ParticleIndices = NewIndices;
		MaxActiveParticles = NewMaxActiveParticles;
	}
	if (bSetMaxActiveCount && SpriteTemplate)
	{
		SpriteTemplate->PeakActiveParticles = std::max(SpriteTemplate->PeakActiveParticles, MaxActiveParticles);
	}
	return true;
}

void FParticleEmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	// 1. 시간과 이미터 위치를 갱신하고 허용된 경우 새 파티클을 생성한다.
	// 2. Base 값을 복원한 뒤 활성 Update 모듈을 실행하고 이동/회전을 적분한다.
	// 3. 수명이 끝난 파티클을 제거하고 FinalUpdate 및 완료 판정을 수행한다.
	LastDeltaTime = 0.0f;
	if (!bEnabled || !CurrentLODLevel || !CurrentLODLevel->bEnabled ||
		!CurrentLODLevel->RequiredModule || bEmitterIsDone || !std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
	{
		return;
	}
	LastDeltaTime = DeltaTime;
	OldLocation = Location;
	Location = Component ? Component->GetWorldLocation() : Location;
	SecondsSinceCreation += DeltaTime;
	EmitterTime += DeltaTime;
	if (EmitterDuration > 0.0f && EmitterTime >= EmitterDuration)
	{
		LoopCount += static_cast<int32>(EmitterTime / EmitterDuration);
		EmitterTime = std::fmod(EmitterTime, EmitterDuration);
	}
	if (bHaltSpawning && bKillOnDeactivate)
	{
		ActiveParticles = 0; // 인덱스의 순열은 그대로 유효하므로 전체를 빈 슬롯으로 재사용한다.
	}
	if (!bSuppressSpawning && !bHaltSpawning)
	{
		SpawnFraction = Spawn(DeltaTime);
	}

	for (int32 Index = 0; Index < ActiveParticles; ++Index)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleIndices[Index] * ParticleStride);
		if ((Particle.Flags & STATE_Particle_Freeze) != 0)
		{
			continue;
		}
		Particle.Velocity = Particle.BaseVelocity;
		Particle.Size = Particle.BaseSize;
		Particle.Color = Particle.BaseColor;
		Particle.RotationRate = Particle.BaseRotationRate;
		if ((Particle.Flags & STATE_Particle_JustSpawned) == 0)
		{
			Particle.OldLocation = Particle.Location;
			Particle.RelativeTime += Particle.OneOverMaxLifetime * DeltaTime;
		}
	}
	for (UParticleModule* Module : CurrentLODLevel->UpdateModules)
	{
		if (Module && Module->bEnabled)
		{
			Module->Update(this, GetModuleDataOffset(Module), DeltaTime);
		}
	}
	UParticleModuleTypeDataBase* TypeData = CurrentLODLevel->TypeDataModule;
	if (TypeData && TypeData->bEnabled && TypeData->bUpdateModule)
	{
		TypeData->Update(this, TypeDataOffset, DeltaTime);
	}
	for (int32 Index = 0; Index < ActiveParticles; ++Index)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleIndices[Index] * ParticleStride);
		if ((Particle.Flags & (STATE_Particle_Freeze | STATE_Particle_JustSpawned)) == 0)
		{
			if ((Particle.Flags & STATE_Particle_FreezeTranslation) == 0)
			{
				Particle.Location += Particle.Velocity * DeltaTime;
			}
			if ((Particle.Flags & STATE_Particle_FreezeRotation) == 0)
			{
				Particle.Rotation += Particle.RotationRate * DeltaTime;
			}
		}
		// 새 파티클은 PostSpawn에서 생성 후 남은 시간만큼 이미 적분했다.
		Particle.Flags &= ~STATE_Particle_JustSpawned;
	}
	KillParticles();
	for (UParticleModule* Module : CurrentLODLevel->Modules)
	{
		if (Module && Module->bEnabled && Module->bFinalUpdateModule)
		{
			Module->FinalUpdate(this, GetModuleDataOffset(Module), DeltaTime);
		}
	}
	if (TypeData && TypeData->bEnabled && TypeData->bFinalUpdateModule)
	{
		TypeData->FinalUpdate(this, TypeDataOffset, DeltaTime);
	}
	bEmitterIsDone = bHaltSpawning && ActiveParticles == 0;
}

uint32 FParticleEmitterInstance::GetModuleDataOffset(UParticleModule* Module)
{
	// 1. 모듈 포인터를 키로 템플릿에 캐시된 파티클별 페이로드 위치를 찾는다.
	// 2. 반환값은 ParticleData 전체가 아닌 개별 파티클 시작 주소 기준이다.
	// 3. 템플릿이나 해당 페이로드가 없으면 계약에 따라 0을 반환한다.
	return SpriteTemplate && Module ? SpriteTemplate->ModuleOffsetMap.FindRef(Module) : 0;
}

float FParticleEmitterInstance::Spawn(float DeltaTime)
{
	// 1. 초당 생성률 * DeltaTime에 이전 프레임의 소수 생성량을 더한다.
	// 2. 정수 부분만 SpawnParticles로 만들고 프레임 내 생성 간격을 전달한다.
	// 3. 소수 부분을 보존해 낮은 생성률도 여러 프레임에 걸쳐 정확히 누적한다.
	UParticleModuleSpawn* SpawnModule = CurrentLODLevel ? CurrentLODLevel->SpawnModule : nullptr;
	if (!bEnabled || bHaltSpawning || !SpawnModule || !SpawnModule->bEnabled ||
		!std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
	{
		return SpawnFraction;
	}
	const float Rate = SpawnModule->Rate;
	if (!std::isfinite(Rate) || Rate <= 0.0f)
	{
		return SpawnFraction;
	}
	const double Accumulated = static_cast<double>(SpawnFraction) + static_cast<double>(Rate) * DeltaTime;
	const double WholeParticles = std::floor(Accumulated);
	const float Remainder = static_cast<float>(Accumulated - WholeParticles);
	const int32 Count = static_cast<int32>(std::min(WholeParticles, static_cast<double>(MaxParticleCapacity)));
	if (Count > 0)
	{
		const float Increment = 1.0f / Rate;
		// StartTime은 첫 파티클이 생성된 뒤 프레임 끝까지 남은 시간이다.
		const float StartTime = std::clamp(DeltaTime - (1.0f - SpawnFraction) * Increment, 0.0f, DeltaTime);
		const bool bLocalSpace = CurrentLODLevel->RequiredModule && CurrentLODLevel->RequiredModule->bUseLocalSpace;
		SpawnParticles(Count, StartTime, Increment, bLocalSpace ? FVector(0.0f) : Location, FVector(0.0f), nullptr);
	}
	SpawnFraction = Remainder; // 용량 초과분은 이월하지 않고 소수 부분만 유지한다.
	return SpawnFraction;
}

void FParticleEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload)
{
	// 1. 풀을 필요에 따라 확장하고 비활성 구간의 첫 슬롯을 하나씩 가져온다.
	// 2. 슬롯 전체를 초기화한 뒤 Spawn 모듈에 파티클과 페이로드 offset을 전달한다.
	// 3. 생성 순번과 프레임 내 생성 시각을 반영하고 활성 개수를 늘린다.
	(void)EventPayload; // 이벤트 생성 모듈은 현재 공통 계약에 없다.
	if (!bEnabled || !CurrentLODLevel || !CurrentLODLevel->bEnabled || Count <= 0 || ParticleStride <= 0)
	{
		return;
	}
	Count = std::min(Count, MaxParticleCapacity - ActiveParticles);
	const int32 RequiredCount = ActiveParticles + Count;
	if (RequiredCount > MaxActiveParticles)
	{
		const int32 GrownCount = std::min(MaxParticleCapacity, std::max(RequiredCount, MaxActiveParticles + std::max(16, MaxActiveParticles / 2)));
		if (!Resize(GrownCount) && !Resize(RequiredCount))
		{
			Count = std::min(Count, MaxActiveParticles - ActiveParticles);
		}
	}
	UParticleModuleTypeDataBase* TypeData = CurrentLODLevel->TypeDataModule;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FBaseParticle* Particle = reinterpret_cast<FBaseParticle*>(ParticleData + ParticleIndices[ActiveParticles] * ParticleStride);
		PreSpawn(Particle, InitialLocation, InitialVelocity);
		const float SpawnTime = std::max(0.0f, StartTime - Increment * Index);
		for (UParticleModule* Module : CurrentLODLevel->SpawnModules)
		{
			if (Module && Module->bEnabled)
			{
				Module->Spawn(this, GetModuleDataOffset(Module), SpawnTime, Particle);
			}
		}
		if (TypeData && TypeData->bEnabled && TypeData->bSpawnModule)
		{
			TypeData->Spawn(this, TypeDataOffset, SpawnTime, Particle);
		}
		const float Interpolation = LastDeltaTime > 0.0f ? std::clamp(1.0f - SpawnTime / LastDeltaTime, 0.0f, 1.0f) : 1.0f;
		PostSpawn(Particle, Interpolation, SpawnTime);
		++ActiveParticles;
		++ParticleCounter;
	}
	if (Count > 0)
	{
		bEmitterIsDone = false;
	}
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	// 1. 재사용 슬롯의 본문, 모든 모듈 페이로드, 패딩을 0으로 지운다.
	// 2. 초기 위치/속도와 기본 크기 1, 흰색, 수명 1초를 설정한다.
	// 3. 생성 순번과 JustSpawned 플래그를 기록해 새 파티클을 구분한다.
	std::memset(Particle, 0, ParticleStride);
	Particle->Location = Particle->OldLocation = InitialLocation;
	Particle->Velocity = Particle->BaseVelocity = InitialVelocity;
	Particle->Size = Particle->BaseSize = FVector(1.0f);
	Particle->Color = Particle->BaseColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	Particle->OneOverMaxLifetime = 1.0f;
	Particle->Flags = static_cast<int32>(ParticleCounter & STATE_CounterMask) | STATE_Particle_JustSpawned;
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime)
{
	// 1. 월드 공간 방출이면 이미터의 이전/현재 위치 사이에서 생성 위치를 보정한다.
	// 2. 새 파티클을 생성 후 프레임 끝까지 남은 SpawnTime만큼만 진행시킨다.
	// 3. JustSpawned를 유지하여 같은 Tick에서 수명과 이동을 두 번 적분하지 않는다.
	const bool bLocalSpace = CurrentLODLevel && CurrentLODLevel->RequiredModule && CurrentLODLevel->RequiredModule->bUseLocalSpace;
	if (!bLocalSpace)
	{
		Particle->Location += (OldLocation - Location) * (1.0f - std::clamp(InterpolationPercentage, 0.0f, 1.0f));
	}
	Particle->OldLocation = Particle->Location;
	Particle->Flags |= STATE_Particle_JustSpawned;
	if ((Particle->Flags & STATE_Particle_Freeze) == 0)
	{
		Particle->RelativeTime += Particle->OneOverMaxLifetime * SpawnTime;
		if ((Particle->Flags & STATE_Particle_FreezeTranslation) == 0)
		{
			Particle->Location += Particle->Velocity * SpawnTime;
		}
		if ((Particle->Flags & STATE_Particle_FreezeRotation) == 0)
		{
			Particle->Rotation += Particle->RotationRate * SpawnTime;
		}
	}
}

void FParticleEmitterInstance::KillParticles()
{
	// 1. 활성 인덱스 구간을 뒤에서부터 순회하며 정규화 수명을 검사한다.
	// 2. RelativeTime >= 1인 파티클을 KillParticle로 활성 목록에서 제거한다.
	// 3. 역순 순회하므로 swap-remove로 들어온 파티클을 빠뜨리지 않는다.
	for (int32 Index = ActiveParticles - 1; Index >= 0; --Index)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleIndices[Index] * ParticleStride);
		if (Particle.RelativeTime >= 1.0f)
		{
			KillParticle(Index);
		}
	}
}

void FParticleEmitterInstance::KillParticle(int32 Index)
{
	// 1. Index는 물리 슬롯 번호가 아니라 활성 인덱스 배열 안의 위치다.
	// 2. 제거할 항목과 마지막 활성 항목의 슬롯 번호만 교환한다.
	// 3. 활성 개수를 줄이면 제거 슬롯이 빈 구간으로 넘어가 O(1)에 재사용된다.
	if (Index < 0 || Index >= ActiveParticles)
	{
		return;
	}
	std::swap(ParticleIndices[Index], ParticleIndices[ActiveParticles - 1]);
	--ActiveParticles;
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
	Data.SubImages_Horizontal = std::max(1, Required->SubImagesHorizontal); 
	Data.SubImages_Vertical = std::max(1, Required->SubImagesVertical);   
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
