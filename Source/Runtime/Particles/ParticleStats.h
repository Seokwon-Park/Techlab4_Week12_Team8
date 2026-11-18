#pragma once
#include "Core/Stats/LightweightStats.h"

// stat particles의 CPU 시간 항목. 개수(파티클 수, 드로우 콜)는 뷰마다 FRenderStats에 모은다.
namespace ParticleStats
{
	// PSC::TickComponent 전체: 이미터 Tick(스폰·모듈·적분·Kill) + 렌더 데이터 복사(UpdateDynamicData)
	DECLARE_CYCLE_STAT("Particle: Simulate", STAT_ParticleSimulate);
	// 프록시 GetDynamicMeshElements: 정렬 + VB 채우기 + 패킷 만들기 (뷰마다 한 번)
	DECLARE_CYCLE_STAT("Particle: Render Data", STAT_ParticleRenderData);
}
