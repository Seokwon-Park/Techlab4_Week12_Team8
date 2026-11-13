#include "EnginePCH.h"
#include "Particles/ParticleHelper.h"
#include "Render/RenderPacket.h"
#include <algorithm>

void FDynamicSpriteEmitterData::Init(bool bInSelected)
{
	bSelected = bInSelected;
	bValid = Source.ActiveParticleCount > 0
		&& Source.DataContainer.ParticleData != nullptr
		&& Source.MaterialInterface != nullptr;
}