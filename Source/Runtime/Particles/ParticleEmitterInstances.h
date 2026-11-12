#pragma once

// ============================================================================
// [Week12 0단계 공통 계약] UE의 Engine/Public/ParticleEmitterInstances.h 에 대응한다.
//
// 데이터 필드는 매크로(BEGIN_UPDATE_LOOP)와 모듈이 직접 읽으므로 이름을 바꾸지 않는다.
// 함수는 인터페이스만 있다. 본문은 ParticleEmitterInstances.cpp의 TODO를 담당자가 채운다.
// ============================================================================

#include "Particles/ParticleHelper.h"

class UParticleEmitter;
class UParticleLODLevel;
class UParticleModule;
class UParticleSystemComponent;
class UMaterialInterface;
struct FParticleEventInstancePayload;

/*-----------------------------------------------------------------------------
	FParticleEmitterInstance
-----------------------------------------------------------------------------*/
struct FParticleEmitterInstance
{
public:
	/** The template this instance is based on.							*/
	UParticleEmitter* SpriteTemplate;
	/** The component who owns it.										*/
	UParticleSystemComponent* Component;
	/** The currently set LOD level.									*/
	UParticleLODLevel* CurrentLODLevel;
	/** The index of the currently set LOD level.						*/
	int32 CurrentLODLevelIndex;
	/** The offset to the TypeData payload in the particle data.		*/
	int32 TypeDataOffset;
	/** The offset to the TypeData instance payload.					*/
	int32 TypeDataInstanceOffset;
	/** The offset to the SubUV payload in the particle data.			*/
	int32 SubUVDataOffset;
	/** The offset to the particle data. 첫 페이로드의 offset			*/
	int32 PayloadOffset;
	/** The location of the emitter instance							*/
	FVector Location;
	/** If true, the emitter is enabled.								*/
	uint8 bEnabled : 1;
	/** If true, kill this emitter instance when it is deactivated.		*/
	uint8 bKillOnDeactivate:1;
	/** if true, kill this emitter instance when it has completed.		*/
	uint8 bKillOnCompleted:1;
	/** Whether this emitter requires sorting as specified by artist.	*/
	uint8 bRequiresSorting:1;
	/** If true, halt spawning for this instance.						*/
	uint8 bHaltSpawning : 1;
	/** true if the emitter has no active particles and will no longer spawn any in the future */
	uint8 bEmitterIsDone:1;
	/** The sort mode to use for this emitter as specified by artist. EParticleSortMode */
	int32 SortMode;
	/** Pointer to the particle data array. 16바이트 정렬 블록. i번째 칸은 ParticleData + i * ParticleStride */
	uint8* ParticleData;
	/** Pointer to the particle index array. 앞쪽 ActiveParticles개가 활성 파티클의 칸 번호다 */
	uint16* ParticleIndices;
	/** Pointer to the instance data array.								*/
	uint8* InstanceData;
	/** The size of the Instance data array.							*/
	int32 InstancePayloadSize;
	/** The total size of a particle (in bytes).						*/
	int32 ParticleSize;
	/** The stride between particles in the ParticleData array.			*/
	int32 ParticleStride;
	/** The number of particles currently active in the emitter.		*/
	int32 ActiveParticles;
	/** Monotonically increasing counter. */
	uint32 ParticleCounter;
	/** The maximum number of active particles that can be held in
	 *	the particle data array. ParticleIndices가 uint16이므로 65535 이하
	 */
	int32 MaxActiveParticles;
	/** The fraction of time left over from spawning.					*/
	float SpawnFraction;
	/** The number of seconds that have passed since the instance was
	 *	created.
	 */
	float SecondsSinceCreation;
	/** */
	float EmitterTime;
	/** The amount of time simulated in the previous time step. */
	float LastDeltaTime;
	/** The previous location of the instance.							*/
	FVector OldLocation;
	/** The number of loops completed by the instance.					*/
	int32 LoopCount;
	/** The current duration fo the emitter instance.					*/
	float EmitterDuration;
	/** The material to render this instance with.						*/
	UMaterialInterface* CurrentMaterial;

	/** Constructor	*/
	FParticleEmitterInstance();

	/** Destructor	*/
	virtual ~FParticleEmitterInstance();

	// SpriteTemplate, Component를 받아 둔다.
	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent);
	// 템플릿에서 ParticleSize를 읽어 16의 배수로 올리고 ParticleStride를 정한 뒤 메모리를 준비한다.
	virtual void Init();

	/**
	 * Ensures enough memory is allocated for the requested number of particles.
	 * ParticleData는 16바이트 정렬(_aligned_malloc, PARTICLE_ALIGNMENT)로 잡는다.
	 *
	 * @param NewMaxActiveParticles		The number of particles for which memory must be allocated.
	 * @param bSetMaxActiveCount		If true, update the peak active particles for this LOD.
	 * @returns bool					true if memory is allocated for at least NewMaxActiveParticles.
	 */
	virtual bool Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount = true);

	// 생성 -> Base 값 복원 -> Update 모듈 -> 위치 적분 -> 수명이 끝난 파티클 Kill
	virtual void Tick(float DeltaTime, bool bSuppressSpawning);

	// 현재 템플릿의 ModuleOffsetMap에서 모듈의 페이로드 offset을 찾는다. 없으면 0
	uint32 GetModuleDataOffset(UParticleModule* Module);

	// 이번 프레임에 생성할 개수를 정해 SpawnParticles()를 호출한다. 남은 SpawnFraction을 돌려준다.
	virtual float Spawn(float DeltaTime);

	/**
	 * Spawn the indicated number of particles.
	 *
	 * @param Count The number of particles to spawn.
	 * @param StartTime			The local emitter time at which to begin spawning particles.
	 * @param Increment			The time delta between spawned particles.
	 * @param InitialLocation	The initial location of spawned particles.
	 * @param InitialVelocity	The initial velocity of spawned particles.
	 * @param EventPayload		Event generator payload if events should be triggered.
	 */
	void SpawnParticles( int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload );

	/**
	 *	Handle any pre-spawning actions required for particles
	 */
	virtual void PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity);

	/**
	 *	Handle any post-spawning actions required by the instance
	 */
	virtual void PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime);

	// RelativeTime이 1을 넘긴 파티클을 모두 제거한다.
	virtual void KillParticles();

	/**
	 *	Kill the particle at the given instance. (팀 합의: swap-remove)
	 *
	 *	@param	Index		The index of the particle to kill. (ParticleIndices에서의 위치)
	 */
	virtual void KillParticle(int32 Index);

	/**
	 *	Retrieves the dynamic data for the emitter. new로 만들어 돌려준다. 그릴 것이 없으면 nullptr
	 *	(UE는 ERHIFeatureLevel 인자도 받는다. 이 엔진에는 그 타입이 없어 뺐다)
	 */
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected)
	{
		return nullptr;
	}

	/**
	 *	Retrieves replay data for the emitter
	 *
	 *	@return	The replay data, or NULL on failure
	 */
	virtual FDynamicEmitterReplayDataBase* GetReplayData()
	{
		return nullptr;
	}

	/**
	 * Captures dynamic replay data for this particle system.
	 * 현재 파티클 메모리와 인덱스 배열을 OutData.DataContainer로 복사한다.
	 *
	 * @param	OutData		[Out] Data will be copied here
	 *
	 * @return Returns true if successful
	 */
	virtual bool FillReplayData( FDynamicEmitterReplayDataBase& OutData );
};

/*-----------------------------------------------------------------------------
	ParticleSpriteEmitterInstance
-----------------------------------------------------------------------------*/
struct FParticleSpriteEmitterInstance : public FParticleEmitterInstance
{
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected) override;
	virtual FDynamicEmitterReplayDataBase* GetReplayData() override;
	virtual bool FillReplayData( FDynamicEmitterReplayDataBase& OutData ) override;
};

/*-----------------------------------------------------------------------------
	ParticleMeshEmitterInstance
-----------------------------------------------------------------------------*/
struct FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
	virtual FDynamicEmitterDataBase* GetDynamicData(bool bSelected) override;
	virtual FDynamicEmitterReplayDataBase* GetReplayData() override;
	virtual bool FillReplayData( FDynamicEmitterReplayDataBase& OutData ) override;
};
