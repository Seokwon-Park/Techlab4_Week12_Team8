#include "EnginePCH.h"
#include "ParticleSystemComponent.h"
#include "Engine/ParticleSystemSceneProxy.h"
#include "Particles/ParticleModuleRequired.h"
#include "Asset/AssetManager.h"

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
		Src.SortMode = PSORTMODE_DistanceToView;
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
	ClearDynamicData();
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

	Super::BeginPlay();
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	////////////////////파티클 테스팅 용 코드(지워도 됨)
	if (bUseRenderTestData)
	{
		if (!RenderTestMaterial)
		{
			// SubUVMaterial을 복제한다. 텍스처는 그 에셋의 StarParticle.png를 그대로 쓴다
			UMaterial* Base = UAssetManager::GetAssetByPath<UMaterial>("SubUVMaterial");
			RenderTestMaterial = UMaterial::CreateInstance(Base);
			if (RenderTestMaterial)
			{
				RenderTestMaterial->PSOType = EPSOType::ParticleSprite_Additive;   // AlphaBlend로 바꿔 가며 비교
				RenderTestMaterial->SamplerState = ESamplerState::LinearClamp;
			}
		}
		RenderTestTime += DeltaTime;
		ClearDynamicData();
		if (RenderTestMaterial)
		{
			FDynamicSpriteEmitterData* Data = MakeTestSpriteData(RenderTestMaterial, 32, RenderTestTime);
			Data->EmitterIndex = 0;
			EmitterRenderData.Add(Data);
		}
		return;
	}
	////////////////////파티클 테스팅 용 코드(지워도 됨)

	UpdateDynamicData();
}

void UParticleSystemComponent::OnUnregister()
{
	ResetParticles();
	ClearDynamicData();
	Super::OnUnregister();
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}

FBox UParticleSystemComponent::CalcLocalBounds() const
{
	// 지금은 FBox() (크기 0) → BVH 컬링으로 항상 사라진다.
	// 1번이 파티클 위치로 계산하기 전까지는 넉넉한 고정 상자를 쓴다.
	return FBox{ FVector(-10.0f, -10.0f, -10.0f), FVector(10.0f, 10.0f, 10.0f) };
}

void UParticleSystemComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);   // 빠지면 bCastShadow 등을 바꿔도 MarkRenderStateDirty가 안 불린다
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
