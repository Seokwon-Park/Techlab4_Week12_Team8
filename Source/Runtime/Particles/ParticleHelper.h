#pragma once

// ============================================================================
// [Week12 0단계 공통 계약] UE의 Engine/Public/ParticleHelper.h 에 대응한다.
//
// 구성과 이름, 필드 순서는 UE 원본을 따른다. 원본에서 이번 과제 범위 밖인 필드와 함수는 뺐다.
// 모듈 호출 형태는 발제문과 같은 UE4 방식(Owner, Offset 인자)이다. UE5의 Context 구조체 방식이 아니다.
// 레이아웃(필드 순서, 크기, 정렬)을 바꾸려면 반드시 팀 합의 후 수정한다.
// ============================================================================

#include <type_traits>
#include "Core/Types.h"
#include "Containers/Array.h"
#include "Math/Vector.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"
#include "Math/Matrix.h"
#include "Math/Color.h"

class UMaterialInterface;
class UParticleModuleRequired;
class UStaticMesh;
struct FSceneView;
struct FParticleRequiredModule;
struct FParticleMeshEmitterInstance;

/*-----------------------------------------------------------------------------
	정렬 기준
	- 파티클 메모리 블록은 16바이트 정렬로 할당한다.
	- FParticleEmitterInstance::Init()이 ParticleSize를 16의 배수로 올림하고 ParticleStride에 넣는다.
	- 따라서 모든 파티클의 시작 주소는 16바이트 경계에 놓인다.
	UE는 Align(ParticleSize, 16)을 쓴다. 이 엔진에는 Align이 없어 아래 두 가지만 따로 둔다.
-----------------------------------------------------------------------------*/
inline constexpr int32 PARTICLE_ALIGNMENT = 16;

constexpr int32 AlignParticleBytes(int32 Bytes)
{
	return (Bytes + (PARTICLE_ALIGNMENT - 1)) & ~(PARTICLE_ALIGNMENT - 1);
}

/*-----------------------------------------------------------------------------
	Helper macros.
	Update 계열은 모듈의 Update(Owner, Offset, DeltaTime) 안에서,
	SPAWN_INIT은 Spawn(Owner, Offset, SpawnTime, ParticleBase) 안에서 쓴다.
-----------------------------------------------------------------------------*/
#define DECLARE_PARTICLE(Name,Address)		\
	FBaseParticle& Name = *((FBaseParticle*) (Address));

#define DECLARE_PARTICLE_CONST(Name,Address)		\
	const FBaseParticle& Name = *((const FBaseParticle*) (Address));

#define DECLARE_PARTICLE_PTR(Name,Address)		\
	FBaseParticle* Name = (FBaseParticle*) (Address);

#define BEGIN_UPDATE_LOOP																								\
	{																													\
		int32&			ActiveParticles = Owner->ActiveParticles;														\
		uint32			CurrentOffset	= Offset;																		\
		const uint8*	ParticleData	= Owner->ParticleData;															\
		const uint32	ParticleStride	= Owner->ParticleStride;														\
		uint16*			ParticleIndices	= Owner->ParticleIndices;														\
		for(int32 i=ActiveParticles-1; i>=0; i--)																		\
		{																												\
			const int32	CurrentIndex	= ParticleIndices[i];															\
			const uint8* ParticleBase	= ParticleData + CurrentIndex * ParticleStride;									\
			FBaseParticle& Particle		= *((FBaseParticle*) ParticleBase);												\
			if ((Particle.Flags & STATE_Particle_Freeze) == 0)															\
			{																											\

#define END_UPDATE_LOOP																									\
			}																											\
			CurrentOffset				= Offset;																		\
		}																												\
	}

#define CONTINUE_UPDATE_LOOP																							\
		CurrentOffset = Offset;																							\
		continue;

#define SPAWN_INIT																										\
	const int32		ActiveParticles	= Owner->ActiveParticles;															\
	const uint32	ParticleStride	= Owner->ParticleStride;															\
	uint32			CurrentOffset	= Offset;																			\
	FBaseParticle&	Particle		= *(ParticleBase);

// 현재 파티클에서 이 모듈의 페이로드를 Type&로 꺼내고 CurrentOffset을 그만큼 전진시킨다.
#define PARTICLE_ELEMENT(Type,Name)																						\
	Type& Name = *((Type*)((uint8*)ParticleBase + CurrentOffset));														\
	CurrentOffset += sizeof(Type);

// BEGIN_UPDATE_LOOP 안에서 현재 파티클을 제거한다.
#define KILL_CURRENT_PARTICLE																							\
	{																													\
		ParticleIndices[i]					= ParticleIndices[ActiveParticles-1];										\
		ParticleIndices[ActiveParticles-1]	= CurrentIndex;																\
		ActiveParticles--;																								\
	}

/*-----------------------------------------------------------------------------
	FBaseParticle
	UE 원본과 필드 순서가 같다. 이 엔진의 FVector는 float 3개(12바이트)라 총 136바이트다.
	매 Tick 시작에 코어가 Velocity = BaseVelocity, Size = BaseSize, Color = BaseColor,
	RotationRate = BaseRotationRate 로 되돌린 뒤 Update 모듈을 실행한다.

	페이로드 offset 규칙:
	  [ FBaseParticle (136) | 모듈 A 페이로드 | 모듈 B 페이로드 | ... | 패딩 ]
	  ^ 파티클 시작                                                   ^ ParticleStride (16의 배수)
	- 모듈은 RequiredBytes()로 파티클당 페이로드 바이트 수를 알린다. 0이면 페이로드가 없다.
	- offset은 파티클 시작 주소 기준이고 UParticleEmitter::CacheEmitterModuleInfo()가 ModuleOffsetMap에 기록한다.
	- 코어는 FParticleEmitterInstance::GetModuleDataOffset(Module)을 Offset 인자로 넘긴다. 페이로드가 없으면 0이다.
-----------------------------------------------------------------------------*/
struct FBaseParticle
{
	// 24 bytes
	FVector			OldLocation;			// Last frame's location, used for collision
	FVector			Location;				// Current location

	// 16 bytes
	FVector			BaseVelocity;			// Velocity = BaseVelocity at the start of each frame.
	float			Rotation;				// Rotation of particle (in Radians)

	// 16 bytes
	FVector			Velocity;				// Current velocity, gets reset to BaseVelocity each frame to allow
	float			BaseRotationRate;		// Initial angular velocity of particle (in Radians per second)

	// 16 bytes
	FVector			BaseSize;				// Size = BaseSize at the start of each frame
	float			RotationRate;			// Current rotation rate, gets reset to BaseRotationRate each frame

	// 16 bytes
	FVector			Size;					// Current size, gets reset to BaseSize each frame
	int32			Flags;					// Flags indicating various particle states

	// 16 bytes
	FLinearColor	Color;					// Current color of particle.

	// 16 bytes
	FLinearColor	BaseColor;				// Base color of the particle

	// 16 bytes
	float			RelativeTime;			// Relative time, range is 0 (==spawn) to 1 (==death)
	float			OneOverMaxLifetime;		// Reciprocal of lifetime
	float			Placeholder0;
	float			Placeholder1;
};
static_assert(sizeof(FBaseParticle) == 136, "FBaseParticle 레이아웃이 바뀌었다. 팀 합의 후 이 값을 고친다.");
static_assert(std::is_trivially_copyable_v<FBaseParticle>, "파티클은 memcpy로 복사한다.");

/*-----------------------------------------------------------------------------
	Particle State Flags
-----------------------------------------------------------------------------*/
enum EParticleStates
{
	/** Ignore updates to the particle						*/
	STATE_Particle_JustSpawned			= 0x02000000,
	/** Ignore updates to the particle						*/
	STATE_Particle_Freeze				= 0x04000000,
	/** Ignore collision updates to the particle			*/
	STATE_Particle_IgnoreCollisions		= 0x08000000,
	/**	Stop translations of the particle					*/
	STATE_Particle_FreezeTranslation	= 0x10000000,
	/**	Stop rotations of the particle						*/
	STATE_Particle_FreezeRotation		= 0x20000000,
	/** Combination for a single check of 'ignore' flags	*/
	STATE_Particle_CollisionIgnoreCheck	= STATE_Particle_Freeze |STATE_Particle_IgnoreCollisions | STATE_Particle_FreezeTranslation| STATE_Particle_FreezeRotation,
	/** Delay collision updates to the particle				*/
	STATE_Particle_DelayCollisions		= 0x40000000,
	/** Flag indicating the particle has had at least one collision	*/
	STATE_Particle_CollisionHasOccurred	= 0x80000000,
};

/*-----------------------------------------------------------------------------
	Particle vertex formats. (렌더러 담당 소유. 셰이더 입력과 맞춘다)
-----------------------------------------------------------------------------*/
// Per-particle data sent to the GPU.
struct FParticleSpriteVertex
{
	/** The position of the particle. */
	FVector Position;
	/** The relative time of the particle. */
	float RelativeTime;
	/** The previous position of the particle. */
	FVector	OldPosition;
	/** Value that remains constant over the lifetime of a particle. */
	float ParticleId;
	/** The size of the particle. */
	FVector2 Size;
	/** The rotation of the particle. */
	float Rotation;
	/** The sub-image index for the particle. */
	float SubImageIndex;
	/** The color of the particle. */
	FLinearColor Color;
};

// Per-particle data sent to the GPU.
struct FMeshParticleInstanceVertex
{
	/** The color of the particle. */
	FLinearColor Color;
	/** The instance to world transform of the particle. Translation vector is packed into W components. */
	FVector4 Transform[3];
	/** The velocity of the particle, XYZ: direction, W: speed. */
	FVector4 Velocity;
	/** The sub-image texture offsets for the particle. */
	int16 SubUVParams[4];
	/** The sub-image lerp value for the particle. */
	float SubUVLerp;
	/** The relative time of the particle. */
	float RelativeTime;
};

// SortSpriteParticles()의 결과 한 칸
struct FParticleOrder
{
	int32 ParticleIndex;

	union
	{
		float Z;
		uint32 C;
	};

	FParticleOrder(int32 InParticleIndex, float InZ) :
		ParticleIndex(InParticleIndex),
		Z(InZ)
	{}

	FParticleOrder(int32 InParticleIndex, uint32 InC) :
		ParticleIndex(InParticleIndex),
		C(InC)
	{}
};

/*-----------------------------------------------------------------------------
	Dynamic emitter data: 코어 -> 렌더러 경계

	코어가 채운다:
	  FParticleEmitterInstance::GetDynamicData()가 FDynamic*EmitterData를 new로 만들고
	  FillReplayData()로 Source(ReplayData)에 현재 파티클 메모리를 복사한다.
	  UParticleSystemComponent::EmitterRenderData가 이 포인터들을 소유한다.

	렌더러가 읽는다:
	  GetSource()로 ReplayData만 본다. FParticleEmitterInstance와 모듈에는 접근하지 않는다.
	  정렬, 정점 버퍼 채우기, 드로우 콜이 렌더러의 일이다. ReplayData는 수정하지 않는다.
-----------------------------------------------------------------------------*/
enum EDynamicEmitterType
{
	DET_Unknown = 0,
	DET_Sprite,
	DET_Mesh,
	DET_Beam2,
	DET_Ribbon,
	DET_AnimTrail,
	DET_Custom
};

// [ ParticleData : ParticleDataNumBytes | ParticleIndices : ParticleIndicesNumShorts * 2 ]
struct FParticleDataContainer
{
	int32 MemBlockSize;
	int32 ParticleDataNumBytes;
	int32 ParticleIndicesNumShorts;
	uint8* ParticleData; // this is also the memory block we allocated
	uint16* ParticleIndices; // not allocated, this is at the end of the memory block

	FParticleDataContainer()
		: MemBlockSize(0)
		, ParticleDataNumBytes(0)
		, ParticleIndicesNumShorts(0)
		, ParticleData(nullptr)
		, ParticleIndices(nullptr)
	{
	}
	~FParticleDataContainer()
	{
		Free();
	}
	FParticleDataContainer(const FParticleDataContainer&) = delete;
	FParticleDataContainer& operator=(const FParticleDataContainer&) = delete;

	// 0단계에서 구현해 둔 유일한 함수다 (ParticleEmitterInstances.cpp). 16바이트 정렬로 할당한다.
	void Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts);
	void Free();
};

/** Source data base class for all emitter types */
struct FDynamicEmitterReplayDataBase
{
	/**	The type of emitter. */
	EDynamicEmitterType	eEmitterType;

	/**	The number of particles currently active in this emitter. */
	int32 ActiveParticleCount;

	int32 ParticleStride;

	// i번째 활성 파티클 = ParticleData + ParticleIndices[i] * ParticleStride  (0 <= i < ActiveParticleCount)
	FParticleDataContainer DataContainer;

	FVector Scale;

	/** Whether this emitter requires sorting as specified by artist.	*/
	int32 SortMode;

	FDynamicEmitterReplayDataBase()
		: eEmitterType( DET_Unknown ),
		  ActiveParticleCount( 0 ),
		  ParticleStride( 0 ),
		  Scale( FVector( 1.0f ) ),
		  SortMode(0)	// Default to PSORTMODE_None
	{
	}

	virtual ~FDynamicEmitterReplayDataBase() = default;
};

/** Base class for Sprite emitters and other emitter types that share similar features. */
struct FDynamicSpriteEmitterReplayDataBase
	: public FDynamicEmitterReplayDataBase
{
	UMaterialInterface*				MaterialInterface;
	struct FParticleRequiredModule	*RequiredModule;	// ParticleModuleRequired.h
	int32							MaxDrawCount;
	int32							SubUVDataOffset;
	int32							SubImages_Horizontal;
	int32							SubImages_Vertical;
	bool						bUseLocalSpace;
	bool						bLockAxis;
	uint8						ScreenAlignment;
	uint8						LockAxisFlag;
	FVector2					PivotOffset;

	FDynamicSpriteEmitterReplayDataBase()
		: MaterialInterface(nullptr)
		, RequiredModule(nullptr)
		, MaxDrawCount(0)
		, SubUVDataOffset(0)
		, SubImages_Horizontal(1)
		, SubImages_Vertical(1)
		, bUseLocalSpace(false)
		, bLockAxis(false)
		, ScreenAlignment(0)
		, LockAxisFlag(0)
		, PivotOffset(-0.5f, -0.5f)
	{
	}
};

/** Base class for all emitter types */
struct FDynamicEmitterDataBase
{
	FDynamicEmitterDataBase(const UParticleModuleRequired* RequiredModule)
		: bSelected(false)
		, bValid(false)
		, EmitterIndex(0)
	{
	}

	virtual ~FDynamicEmitterDataBase() = default;

	/** Returns the source data for this particle system */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;

	/** true if this emitter is currently selected */
	uint32	bSelected:1;
	/** true if this emitter has valid rendering data */
	uint32	bValid:1;

	// UParticleSystemComponent::EmitterInstances 안에서의 번호
	int32  EmitterIndex;
};

/** Base class for Sprite emitters and other emitter types that share similar features. */
struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	FDynamicSpriteEmitterDataBase(const UParticleModuleRequired* RequiredModule) :
		FDynamicEmitterDataBase(RequiredModule)
	{
	}

	/**
	 *	Sort the given sprite particles. 구현은 렌더러 담당.
	 *
	 *	@param	SortMode			The sort mode to utilize (EParticleSortMode)
	 *	@param	bLocalSpace			true if the emitter is using local space
	 *	@param	ParticleCount		The number of particles
	 *	@param	ParticleData		The actual particle data
	 *	@param	ParticleStride		The stride between entries in the ParticleData array
	 *	@param	ParticleIndices		Indirect index list into ParticleData
	 *	@param	View				The scene view being rendered
	 *	@param	LocalToWorld		The local to world transform of the component rendering the emitter
	 *	@param	ParticleOrder		The array to fill in with ordered indices
	 */
	void SortSpriteParticles(int32 SortMode, bool bLocalSpace,
		int32 ParticleCount, const uint8* ParticleData, int32 ParticleStride, const uint16* ParticleIndices,
		const FSceneView* View, const FMatrix& LocalToWorld, FParticleOrder* ParticleOrder) const;

	/**
	 *	Get the vertex stride for the dynamic rendering data
	 *	(UE는 ERHIFeatureLevel 인자를 받는다. 이 엔진에는 그 타입이 없어 뺐다)
	 */
	virtual int32 GetDynamicVertexStride() const = 0;

	/**
	 *	Get the source replay data for this emitter
	 */
	virtual const FDynamicSpriteEmitterReplayDataBase* GetSourceData() const = 0;
};

/** Source data for Sprite emitters */
struct FDynamicSpriteEmitterReplayData
	: public FDynamicSpriteEmitterReplayDataBase
{
	FDynamicSpriteEmitterReplayData()
	{
	}
};

/** Dynamic emitter data for sprite emitters */
struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterData(const UParticleModuleRequired* RequiredModule) :
		FDynamicSpriteEmitterDataBase(RequiredModule)
	{
	}

	/** Initialize this emitter's dynamic rendering data, called after source data has been filled in. 구현은 렌더러 담당 */
	void Init( bool bInSelected );

	virtual int32 GetDynamicVertexStride() const override
	{
		return sizeof(FParticleSpriteVertex);
	}

	virtual const FDynamicSpriteEmitterReplayDataBase* GetSourceData() const override
	{
		return &Source;
	}

	/** Returns the source data for this particle system */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const override
	{
		return Source;
	}

	/** The frame source data for this particle system. */
	FDynamicSpriteEmitterReplayData Source;
};

/** Source data for Mesh emitters */
struct FDynamicMeshEmitterReplayData
	: public FDynamicSpriteEmitterReplayDataBase
{
	bool	bScaleUV;
	int32	MeshRotationOffset;
	uint8	MeshAlignment;
	bool	bMeshRotationActive;
	FVector	LockedAxis;

	FDynamicMeshEmitterReplayData() :
		bScaleUV(false),
		MeshRotationOffset(0),
		MeshAlignment(0),
		bMeshRotationActive(false),
		LockedAxis(1.0f, 0.0f, 0.0f)
	{
	}
};

/** Dynamic emitter data for Mesh emitters */
struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicMeshEmitterData(const UParticleModuleRequired* RequiredModule) :
		FDynamicSpriteEmitterDataBase(RequiredModule),
		StaticMesh(nullptr)
	{
	}

	/** Initialize this emitter's dynamic rendering data, called after source data has been filled in. 구현은 렌더러 담당 */
	void Init(	bool bInSelected,
				const FParticleMeshEmitterInstance* InEmitterInstance,
				UStaticMesh* InStaticMesh);

	virtual int32 GetDynamicVertexStride() const override
	{
		return sizeof(FMeshParticleInstanceVertex);
	}

	virtual const FDynamicSpriteEmitterReplayDataBase* GetSourceData() const override
	{
		return &Source;
	}

	/** Returns the source data for this particle system */
	virtual const FDynamicEmitterReplayDataBase& GetSource() const override
	{
		return Source;
	}

	/** The frame source data for this particle system. */
	FDynamicMeshEmitterReplayData Source;

	UStaticMesh* StaticMesh;
};
