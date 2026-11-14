#include "EnginePCH.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleModule.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"

// [Week12 런타임 코어] LOD 조회와 파티클/인스턴스 페이로드 배치 정보를 관리한다.

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODLevel)
{
	if (LODLevel >= 0 && LODLevel < LODLevels.Num())
	{
		return LODLevels[LODLevel];
	}
	return nullptr;
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	// 1. 이전 캐시를 지운다. 모듈을 삭제하거나 이 함수를 다시 호출해도
	// 이전 offset과 크기가 남거나 새 크기에 중복 누적되지 않도록 한다.
	ModuleOffsetMap.Reset();
	ModuleInstanceOffsetMap.Reset();
	ParticleSize = sizeof(FBaseParticle);
	ReqInstanceBytes = 0;
	TypeDataOffset = 0;
	TypeDataInstanceOffset = -1;

	// 2. 공통 계약에 따라 LOD 0의 구성으로 하나의 메모리 배치를 계산한다.
	// ParticleSize는 파티클 안의 다음 빈 위치, ReqInstanceBytes는
	// InstanceData 안의 다음 빈 위치로 사용한다. 두 공간은 독립적이다.
	UParticleLODLevel* LOD0 = GetLODLevel(0);
	if (!LOD0)
	{
		return;
	}

	// 3. TypeData가 요구하는 공간을 먼저 배정한다.
	// offset을 먼저 기록하고, 요구한 바이트 수만큼 다음 빈 위치를 전진시킨다.
	UParticleModuleTypeDataBase* TypeData = LOD0->TypeDataModule;
	if (TypeData)
	{
		const uint32 ParticleBytes = TypeData->RequiredBytes(TypeData);
		if (ParticleBytes > 0)
		{
			// 각 파티클의 시작 주소 + TypeDataOffset으로 접근한다.
			TypeDataOffset = ParticleSize;
			ParticleSize += static_cast<int32>(ParticleBytes);
		}

		const uint32 InstanceBytes = TypeData->RequiredBytesPerInstance();
		if (InstanceBytes > 0)
		{
			// InstanceData의 시작 주소 + TypeDataInstanceOffset으로 접근한다.
			// 0도 유효한 위치이므로, 공간이 없는 경우의 기본값은 -1이다.
			TypeDataInstanceOffset = ReqInstanceBytes;
			ReqInstanceBytes += static_cast<int32>(InstanceBytes);
		}
	}

	// 4. 일반 모듈은 Modules의 순서대로 배정한다. TypeData는 이 배열에 없다.
	// SpawnModules/UpdateModules는 같은 모듈의 실행 목록이므로 다시 합산하지 않는다.
	// 비활성 모듈도 공간을 확보해 bEnabled 변경 시에도 같은 배치를 유지한다.
	for (UParticleModule* Module : LOD0->Modules)
	{
		if (!Module)
		{
			continue;
		}

		const uint32 ParticleBytes = Module->RequiredBytes(TypeData);
		if (ParticleBytes > 0)
		{
			// 파티클별 Payload가 있는 모듈만 맵에 기록한다.
			ModuleOffsetMap.Add(Module, static_cast<uint32>(ParticleSize));
			ParticleSize += static_cast<int32>(ParticleBytes);
		}

		// 파티클별 Payload가 없어도 인스턴스별 Payload는 필요할 수 있으므로
		// RequiredBytes()의 결과와 별개로 계산한다.
		const uint32 InstanceBytes = Module->RequiredBytesPerInstance();
		if (InstanceBytes > 0)
		{
			ModuleInstanceOffsetMap.Add(Module, static_cast<uint32>(ReqInstanceBytes));
			ReqInstanceBytes += static_cast<int32>(InstanceBytes);
		}
	}

	// 5. 누적 결과가 파티클 하나의 데이터 크기와 InstanceData의 전체 필요 크기다.
	// 여기서는 RequiredBytes*()가 보고한 크기를 순서대로 이어 붙이며,
	// ParticleSize에 마지막 16바이트 올림용 패딩을 추가하지 않는다.
	// 실제 메모리 할당과 ParticleStride = AlignParticleBytes(ParticleSize)는 Init()의 역할이다.
}
