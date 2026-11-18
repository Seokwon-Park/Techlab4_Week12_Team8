#include "EnginePCH.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include <algorithm>
#include <cmath>

// [Week12 런타임 코어] LOD 조회와 파티클/인스턴스 페이로드 배치 정보를 관리한다.

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODLevel) const
{
	return LODLevel >= 0 && LODLevel < static_cast<int32>(LODLevels.Num()) ? LODLevels[LODLevel] : nullptr;
}

bool UParticleEmitter::IsLODLevelCompatible(const UParticleLODLevel* LODLevel) const
{
	const UParticleLODLevel* LOD0 = GetLODLevel(0);
	if (!LOD0 || !LOD0->RequiredModule || !LODLevel || !LODLevel->RequiredModule ||
		LOD0->Modules.Num() != LODLevel->Modules.Num() ||
		LOD0->RequiredModule->bUseLocalSpace != LODLevel->RequiredModule->bUseLocalSpace)
	{
		return false;
	}
	const UClass* BaseType = LOD0->TypeDataModule ? LOD0->TypeDataModule->GetClass() : nullptr;
	const UClass* LODType = LODLevel->TypeDataModule ? LODLevel->TypeDataModule->GetClass() : nullptr;
	if (BaseType != LODType)
	{
		return false;
	}
	for (int32 Index = 0; Index < LOD0->Modules.Num(); ++Index)
	{
		const UParticleModule* BaseModule = LOD0->Modules[Index];
		const UParticleModule* LODModule = LODLevel->Modules[Index];
		if ((BaseModule ? BaseModule->GetClass() : nullptr) != (LODModule ? LODModule->GetClass() : nullptr))
		{
			return false;
		}
	}
	return true;
}

int32 UParticleEmitter::AddLODLevel(UParticleLODLevel* LODLevel, float MinDistance)
{
	if (!LODLevel || !LODLevel->RequiredModule || !std::isfinite(MinDistance) || MinDistance < 0.0f ||
		(LODLevels.IsEmpty() && MinDistance != 0.0f))
	{
		return -1;
	}
	if (!LODLevels.IsEmpty())
	{
		const UParticleLODLevel* Previous = GetLODLevel(static_cast<int32>(LODLevels.Num()) - 1);
		if (!Previous || !std::isfinite(Previous->MinDistance) || MinDistance <= Previous->MinDistance ||
			!IsLODLevelCompatible(LODLevel))
		{
			return -1;
		}
	}
	for (const UParticleLODLevel* Existing : LODLevels)
	{
		if (Existing == LODLevel)
		{
			return -1;
		}
	}
	LODLevel->Level = static_cast<int32>(LODLevels.Num());
	LODLevel->MinDistance = MinDistance;
	LODLevel->UpdateModuleLists();
	LODLevels.Add(LODLevel);
	return LODLevel->Level;
}

int32 UParticleEmitter::SelectLODLevel(float Distance) const
{
	if (!IsLODLevelCompatible(GetLODLevel(0)))
	{
		return -1;
	}
	// NaN은 LOD0으로, 음수는 거리 0으로 취급한다. +Infinity는 가장 먼 LOD다.
	if (std::isnan(Distance) || Distance < 0.0f)
	{
		Distance = 0.0f;
	}
	int32 Selected = 0;
	float LastDistance = 0.0f;
	for (int32 Index = 1; Index < static_cast<int32>(LODLevels.Num()); ++Index)
	{
		const UParticleLODLevel* LOD = LODLevels[Index];
		if (!IsLODLevelCompatible(LOD) || !std::isfinite(LOD->MinDistance) || LOD->MinDistance <= LastDistance)
		{
			continue;
		}
		LastDistance = LOD->MinDistance;
		if (Distance >= LOD->MinDistance)
		{
			Selected = Index;
		}
	}
	return Selected;
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	ModuleOffsetMap.Reset();
	ModuleInstanceOffsetMap.Reset();
	ParticleSize = sizeof(FBaseParticle);
	ReqInstanceBytes = 0;
	TypeDataOffset = 0;
	TypeDataInstanceOffset = -1;

	TArray<UParticleLODLevel*> CompatibleLODs;
	for (UParticleLODLevel* LOD : LODLevels)
	{
		if (IsLODLevelCompatible(LOD))
		{
			LOD->UpdateModuleLists();
			CompatibleLODs.Add(LOD);
		}
	}
	if (CompatibleLODs.IsEmpty())
	{
		return;
	}

	// TypeData도 모든 LOD에서 같은 종류다. 공간은 가장 큰 요구량으로 한 번만 확보한다.
	uint32 TypeBytes = 0;
	uint32 TypeInstanceBytes = 0;
	for (UParticleLODLevel* LOD : CompatibleLODs)
	{
		if (UParticleModuleTypeDataBase* TypeData = LOD->TypeDataModule)
		{
			TypeBytes = std::max(TypeBytes, TypeData->RequiredBytes(TypeData));
			TypeInstanceBytes = std::max(TypeInstanceBytes, TypeData->RequiredBytesPerInstance());
		}
	}
	if (TypeBytes > 0)
	{
		TypeDataOffset = ParticleSize;
		ParticleSize += static_cast<int32>(TypeBytes);
	}
	if (TypeInstanceBytes > 0)
	{
		TypeDataInstanceOffset = ReqInstanceBytes;
		ReqInstanceBytes += static_cast<int32>(TypeInstanceBytes);
	}

	// 같은 모듈 슬롯은 LOD가 바뀌어도 같은 페이로드를 사용한다.
	// LOD별 데이터를 따로 배치하면 기존 파티클의 모듈 상태를 잃게 된다.
	for (int32 Index = 0; Index < CompatibleLODs[0]->Modules.Num(); ++Index)
	{
		uint32 ParticleBytes = 0;
		uint32 InstanceBytes = 0;
		for (UParticleLODLevel* LOD : CompatibleLODs)
		{
			if (UParticleModule* Module = LOD->Modules[Index])
			{
				ParticleBytes = std::max(ParticleBytes, Module->RequiredBytes(LOD->TypeDataModule));
				InstanceBytes = std::max(InstanceBytes, Module->RequiredBytesPerInstance());
			}
		}
		for (UParticleLODLevel* LOD : CompatibleLODs)
		{
			if (UParticleModule* Module = LOD->Modules[Index])
			{
				if (ParticleBytes > 0) ModuleOffsetMap.Add(Module, static_cast<uint32>(ParticleSize));
				if (InstanceBytes > 0) ModuleInstanceOffsetMap.Add(Module, static_cast<uint32>(ReqInstanceBytes));
			}
		}
		ParticleSize += static_cast<int32>(ParticleBytes);
		ReqInstanceBytes += static_cast<int32>(InstanceBytes);
	}
}
