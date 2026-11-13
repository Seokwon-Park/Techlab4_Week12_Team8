#include "EnginePCH.h"
#include "ParticleSystemComponent.h"
#include "Engine/ParticleSystemSceneProxy.h"

namespace
{
	// 임시 코드 !!!!
	static FDynamicSpriteEmitterData* MakeTestSpriteData(UMaterial* Material, int32 Count, float Time)
	{
		FDynamicSpriteEmitterData* Data = new FDynamicSpriteEmitterData(nullptr);
		FDynamicSpriteEmitterReplayData& Src = Data->Source;
		Src.eEmitterType = DET_Sprite;
		Src.ActiveParticleCount = Count;
		Src.ParticleStride = AlignParticleBytes(sizeof(FBaseParticle));   // 136 → 144
		//Src.SortMode = PSORTMODE_DistanceToView;
		Src.MaterialInterface = Material;
		Src.DataContainer.Alloc(Count * Src.ParticleStride, Count);

		for (int32 i = 0; i < Count; ++i)
		{
			uint8* Slot = Src.DataContainer.ParticleData + i * Src.ParticleStride;
			std::memset(Slot, 0, Src.ParticleStride);
			FBaseParticle& P = *reinterpret_cast<FBaseParticle*>(Slot);

			const float A = static_cast<float>(i) / Count * 6.2831853f + Time;
			P.Location = FVector(std::cos(A) * 3.0f, std::sin(A) * 3.0f, 1.0f + 0.5f * std::sin(A * 3.0f));
			P.OldLocation = P.Location;
			P.Size = FVector(0.6f);
			P.Rotation = A;
			P.Color = FLinearColor(1.0f, 0.5f + 0.5f * std::sin(A), 0.2f, 0.8f);
			P.RelativeTime = static_cast<float>(i) / Count;

			Src.DataContainer.ParticleIndices[i] = static_cast<uint16>(Count - 1 - i);   // 일부러 역순
		}

		Data->Init(false);
		return Data;
	}

}

UParticleSystemComponent::UParticleSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bCastShadow = false;
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ResetParticles();
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
}

void UParticleSystemComponent::InitializeSystem()
{
}

void UParticleSystemComponent::InitParticles()
{
}

void UParticleSystemComponent::ResetParticles()
{
}

void UParticleSystemComponent::Activate(bool bReset)
{
}

void UParticleSystemComponent::Deactivate()
{
}

void UParticleSystemComponent::BeginPlay()
{
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	UpdateDynamicData();
}

void UParticleSystemComponent::OnUnregister()
{
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}

FBox UParticleSystemComponent::CalcLocalBounds() const
{
	return FBox();
}

void UParticleSystemComponent::PostEditChangeProperty(const FProperty& Property)
{
}

void UParticleSystemComponent::UpdateDynamicData()
{
	ClearDynamicData();
	EmitterRenderData.Reserve(EmitterInstances.Num());
	for (int32 i = 0; i < EmitterInstances.Num(); ++i)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[i];
		FDynamicEmitterDataBase* Data = Instance ? Instance->GetDynamicData(false) : nullptr;
		if (Data)
		{
			Data->EmitterIndex = i;
		}
		EmitterRenderData.Add(Data);
	}
}

void UParticleSystemComponent::ClearDynamicData()
{
	for (FDynamicEmitterDataBase* Data : EmitterRenderData)
	{
		delete Data;
	}
	EmitterRenderData.Reset();
}

int32 UParticleSystemComponent::GetNumActiveParticles() const
{
	return int32();
}
