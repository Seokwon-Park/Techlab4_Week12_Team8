#include "EnginePCH.h"
#include "ParticleSystemComponent.h"
#include "Engine/ParticleSystemSceneProxy.h"
#include "Particles/ParticleStats.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particles/ParticleEventManager.h"
#include "Engine/World.h"
#include "Engine/SceneManagement.h"
#include "Components/LightComponentBase.h"
#include "Asset/AssetManager.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include <algorithm>
#include <cmath>

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
			P.Location = FVector(std::cos(A) * 3.0f, std::sin(A) * 3.0f, 5.0f + 0.5f * std::sin(A * 3.0f));   // 나선·빔 위쪽
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

	// 테스트용 파티클 칸 하나를 0으로 비우고 돌려준다
	FBaseParticle& TestParticleAt(FDynamicSpriteEmitterReplayDataBase& Src, int32 Slot)
	{
		uint8* Base = Src.DataContainer.ParticleData + Slot * Src.ParticleStride;
		std::memset(Base, 0, Src.ParticleStride);
		return *reinterpret_cast<FBaseParticle*>(Base);
	}

	void AllocTestParticles(FDynamicSpriteEmitterReplayDataBase& Src, UMaterial* Material, int32 Count)
	{
		Src.ActiveParticleCount = Count;
		Src.ParticleStride = AlignParticleBytes(sizeof(FBaseParticle));
		Src.MaterialInterface = Material;
		Src.bUseLocalSpace = true;   // 컴포넌트를 옮기면 같이 따라온다
		Src.DataContainer.Alloc(Count * Src.ParticleStride, Count);
	}

	// Ribbon 정답 모양: 바깥·아래(오래된 점, 흐림)에서 안쪽·위(새 점, 진함)로 감기는 나선. 천천히 돈다.
	// 스폰 번호(Flags)로 순서를 정하는지 보려고 인덱스 배열은 일부러 섞는다 (홀수 칸 먼저, 짝수 칸 나중).
	FDynamicRibbonEmitterData* MakeTestRibbonData(UMaterial* Material, float Time)
	{
		constexpr int32 Count = 160;
		FDynamicRibbonEmitterData* Data = new FDynamicRibbonEmitterData(nullptr);
		FDynamicRibbonEmitterReplayData& Src = Data->Source;
		Src.eEmitterType = DET_Ribbon;
		Src.bConnectToSource = false;
		AllocTestParticles(Src, Material, Count);

		for (int32 i = 0; i < Count; ++i)
		{
			const float T = static_cast<float>(i) / (Count - 1);          // 0 = 가장 오래된 점, 1 = 가장 새 점
			const float Angle = T * 3.0f * 6.2831853f + Time * 1.5f;      // 세 바퀴
			const float Radius = 3.0f - 2.2f * T;
			FBaseParticle& P = TestParticleAt(Src, i);
			P.Location = FVector(std::cos(Angle) * Radius, std::sin(Angle) * Radius, 0.2f + 1.6f * T);
			P.OldLocation = P.Location;
			P.Size = FVector(0.18f);
			P.Color = FLinearColor(0.65f, 0.35f, 1.0f, 0.1f + 0.9f * T);  // 꼬리로 갈수록 투명
			P.Flags = i & STATE_CounterMask;                              // 스폰 순번
		}
		int32 Write = 0;
		for (int32 i = 1; i < Count; i += 2) Src.DataContainer.ParticleIndices[Write++] = static_cast<uint16>(i);
		for (int32 i = 0; i < Count; i += 2) Src.DataContainer.ParticleIndices[Write++] = static_cast<uint16>(i);

		Data->Init(false);
		return Data;
	}

	// Beam 정답 모양: (-2.5,0,1)에서 (2.5,0,1)까지 가는 가는 선. 양 끝은 고정, 가운데만 흔들린다.
	FDynamicBeamEmitterData* MakeTestBeamData(UMaterial* Material, float Time)
	{
		FDynamicBeamEmitterData* Data = new FDynamicBeamEmitterData(nullptr);
		FDynamicBeamEmitterReplayData& Src = Data->Source;
		Src.eEmitterType = DET_Beam;
		AllocTestParticles(Src, Material, 1);
		Src.TargetOffset = FVector(5.0f, 0.0f, 0.0f);
		Src.Segments = 32;
		Src.NoiseAmplitude = 0.25f;
		Src.EmitterTime = Time;

		FBaseParticle& P = TestParticleAt(Src, 0);
		P.Location = FVector(-2.5f, 0.0f, 3.0f);
		P.OldLocation = P.Location;
		P.Size = FVector(0.08f);
		P.Color = FLinearColor(0.6f, 0.85f, 1.0f, 1.0f);
		Src.DataContainer.ParticleIndices[0] = 0;

		Data->Init(false);
		return Data;
	}
}

UParticleSystemComponent::UParticleSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickEnabled(false);
	bCastShadow = false;
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ResetParticles();
	ClearDynamicData();
	delete RenderTestMaterial;   // 테스트 데이터용 사본 (SubUV 컴포넌트와 같은 방식)
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
	if (Template == NewTemplate)
	{
		return;
	}

	const bool bShouldActivate = IsActive() || IsAutoActivate();
	Template = NewTemplate;
	InitializeSystem();
	if (bShouldActivate)
	{
		Activate();
	}
}

void UParticleSystemComponent::InitializeSystem()
{
	InitParticles();
	UpdateLODLevelFromCamera();
	bWasCompleted = false;
	if (!Template || EmitterInstances.IsEmpty())
	{
		Super::Deactivate();
	}
	SetComponentTickEnabled(IsActive());
	UpdateDynamicData();
	OnTransformDirty();
}

void UParticleSystemComponent::InitParticles()
{
	ResetParticles();
	if (!Template)
	{
		return;
	}

	EmitterInstances.Reserve(Template->Emitters.Num());
	for (UParticleEmitter* Emitter : Template->Emitters)
	{
		UParticleLODLevel* LOD = Emitter ? Emitter->GetLODLevel(0) : nullptr;
		FParticleEmitterInstance* Instance = nullptr;
		if (LOD && LOD->RequiredModule)
		{
			if (LOD->TypeDataModule)
			{
				Instance = LOD->TypeDataModule->CreateInstance(Emitter, this);
			}
			if (!Instance)
			{
				Instance = new FParticleSpriteEmitterInstance();
			}
			Instance->InitParameters(Emitter, this);
			Instance->Init();
			Instance->bHaltSpawning = !IsActive();
		}
		// 템플릿과 같은 인덱스를 유지해 렌더 데이터의 이미터 번호가 바뀌지 않게 한다.
		EmitterInstances.Add(Instance);
	}
}

void UParticleSystemComponent::ResetParticles()
{
	ClearDynamicData();
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		delete Instance;
	}
	EmitterInstances.Reset();
	bWasCompleted = false;
	SetComponentTickEnabled(false);
	OnTransformDirty();
}

void UParticleSystemComponent::Activate(bool bReset)
{
	if (!Template)
	{
		ResetParticles();
		Super::Deactivate();
		return;
	}
	if (!bReset && IsActive() && !bWasCompleted && !EmitterInstances.IsEmpty())
	{
		return;
	}
	if (bReset || bWasCompleted || EmitterInstances.IsEmpty())
	{
		InitializeSystem();
	}

	bool bHasEnabledEmitter = false;
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance && Instance->bEnabled && Instance->CurrentLODLevel)
		{
			Instance->bHaltSpawning = false;
			Instance->bEmitterIsDone = false;
			bHasEnabledEmitter = true;
		}
	}
	if (!bHasEnabledEmitter)
	{
		bWasCompleted = true;
		Super::Deactivate();
		SetComponentTickEnabled(false);
		return;
	}

	bWasCompleted = false;
	Super::Activate(true);
}

void UParticleSystemComponent::Deactivate()
{
	Super::Deactivate();
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->bHaltSpawning = true;
			if (Instance->bKillOnDeactivate)
			{
				Instance->ActiveParticles = 0;
			}
			Instance->bEmitterIsDone = Instance->ActiveParticles == 0;
		}
	}
	UpdateCompletion();
	// 생성은 멈추되 이미 살아 있는 파티클은 수명이 끝날 때까지 Tick한다.
	SetComponentTickEnabled(!bWasCompleted);
	UpdateDynamicData();
	OnTransformDirty();
}

void UParticleSystemComponent::BeginPlay()
{
	Super::BeginPlay();
	if (IsAutoActivate())
	{
		Activate();
	}
	else
	{
		Deactivate();
	}
}

void UParticleSystemComponent::UpdateLODLevel(const FVector& ViewLocation)
{
	const float Distance = (ViewLocation - GetWorldLocation()).Size();
	if (!std::isfinite(Distance))
	{
		return;
	}
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance && Instance->bEnabled && Instance->SpriteTemplate)
		{
			const int32 LODIndex = Instance->SpriteTemplate->SelectLODLevel(Distance);
			if (LODIndex >= 0 && LODIndex != Instance->CurrentLODLevelIndex)
			{
				Instance->SetLODLevel(LODIndex);
			}
		}
	}
}

void UParticleSystemComponent::UpdateLODLevelFromCamera()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (World->IsGameWorld())
	{
		if (APlayerController* Controller = World->GetPlayerController())
		{
			APlayerCameraManager* Camera = Controller->GetCameraManager();
			if (Camera && Camera->HasCameraCache())
			{
				UpdateLODLevel(Camera->GetCameraCachePOV().Location);
				return;
			}
		}
	}
	// 에디터는 Tick 전에 활성 뷰포트 위치를 MainCamera에 동기화한다.
	ACameraActor* Camera = World->GetMainCamera();
	if (Camera && Camera->GetCameraComponent())
	{
		UpdateLODLevel(Camera->GetCameraComponent()->GetWorldLocation());
	}
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	SCOPE_CYCLE_COUNTER(ParticleStats::STAT_ParticleSimulate);   // stat particles
	CollisionEvents.Reset();
	if (!std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
	{
		return;
	}

	////////////////////파티클 테스팅 용 코드(지워도 됨)
	// 에디터 툴바의 Test Data: 시뮬레이션 대신 정답 모양을 아는 렌더 데이터를 그린다
	//   이미터 0 = Sprite 고리 (z=5), 1 = Ribbon 나선 (z 0.2~1.8), 2 = Beam 선 (z=3)
	if (bUseRenderTestData)
	{
		if (!RenderTestMaterial)
		{
			UMaterial* Base = UAssetManager::GetAssetByPath<UMaterial>("SubUVMaterial");   // 텍스처: StarParticle.png
			RenderTestMaterial = UMaterial::CreateInstance(Base);
			if (RenderTestMaterial)
			{
				RenderTestMaterial->Textures = { UAssetManager::GetAssetByPath<UTexture2D>("WhiteTexture") };   // 띠가 텍스처 모양에 가려지지 않게
				RenderTestMaterial->PSOType = EPSOType::ParticleSprite_AlphaBlend;   // 띠는 프록시가 Trail PSO로 바꾼다
				RenderTestMaterial->SamplerState = ESamplerState::LinearClamp;
			}
		}
		RenderTestTime += DeltaTime;
		ClearDynamicData();
		if (RenderTestMaterial)
		{
			FDynamicEmitterDataBase* Test[] = {
				MakeTestSpriteData(RenderTestMaterial, 32, RenderTestTime),
				MakeTestRibbonData(RenderTestMaterial, RenderTestTime),
				MakeTestBeamData(RenderTestMaterial, RenderTestTime) };
			for (int32 i = 0; i < 3; ++i)
			{
				Test[i]->EmitterIndex = i;
				EmitterRenderData.Add(Test[i]);
			}
		}
		return;
	}
	////////////////////파티클 테스팅 용 코드(지워도 됨)
	// 에디터 월드는 BeginPlay 없이 직접 TickComponent를 호출한다.
	if (!HasBegunPlay() && !bWasCompleted && EmitterInstances.IsEmpty() && IsAutoActivate() && Template)
	{
		Activate();
	}
	if (bWasCompleted || (!IsActive() && !IsComponentTickEnabled()))
	{
		return;
	}
	UpdateLODLevelFromCamera();

	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			float RemainingDeltaTime = DeltaTime;
			bool bFinishedSpawning = false;
			UParticleModuleRequired* Required = Instance->CurrentLODLevel ? Instance->CurrentLODLevel->RequiredModule : nullptr;
			if (!Instance->bHaltSpawning && Required && Instance->EmitterDuration > 0.0f && Required->EmitterLoops > 0)
			{
				const double TimeUntilCompletion = (static_cast<double>(Required->EmitterLoops) - Instance->LoopCount) *
					Instance->EmitterDuration - Instance->EmitterTime;
				if (TimeUntilCompletion <= DeltaTime)
				{
					// 마지막 루프 경계를 넘는 프레임은 방출 구간과 잔여 수명 갱신 구간으로 나눈다.
					const float SpawningDeltaTime = std::clamp(static_cast<float>(TimeUntilCompletion), 0.0f, DeltaTime);
					if (SpawningDeltaTime > 0.0f)
					{
						Instance->Tick(SpawningDeltaTime, !IsActive());
					}
					bFinishedSpawning = true;
					RemainingDeltaTime -= SpawningDeltaTime;
				}
			}
			if (RemainingDeltaTime > 0.0f)
			{
				Instance->Tick(RemainingDeltaTime, bFinishedSpawning || !IsActive());
			}
			if ((bFinishedSpawning || Instance->bHaltSpawning) && Instance->ActiveParticles == 0)
			{
				Instance->bEmitterIsDone = true;
			}
		}
	}

	if (!CollisionEvents.IsEmpty())
	{
		UWorld* World = GetWorld();
		if (World && World->MyParticleEventManager)
		{
			World->MyParticleEventManager->HandleParticleCollisionEvents(this, CollisionEvents);
		}
	}

	UpdateCompletion();
	UpdateDynamicData();

	// 컴포넌트가 이동하지 않아도 시뮬레이션 후 컬링 경계를 다시 갱신한다.
	OnTransformDirty();
}

void UParticleSystemComponent::OnUnregister()
{
	Super::Deactivate();
	ResetParticles();
	Super::OnUnregister();
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}

FBox UParticleSystemComponent::CalcLocalBounds() const
{
	FBox Bounds{ FVector(0.0f), FVector(0.0f) };
	bool bHasBounds = false;
	const FMatrix LocalToWorld = GetWorldMatrix();
	const FMatrix WorldToLocal = LocalToWorld.Inverse();
	for (const FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance || !Instance->bEnabled || !Instance->CurrentLODLevel || !Instance->CurrentLODLevel->bEnabled ||
			!Instance->CurrentLODLevel->RequiredModule || !Instance->ParticleData || !Instance->ParticleIndices)
		{
			continue;
		}
		const bool bLocalSpace = Instance->CurrentLODLevel->RequiredModule->bUseLocalSpace;
		const auto* MeshTypeData = Cast<UParticleModuleTypeDataMesh>(Instance->CurrentLODLevel->TypeDataModule);
		const UStaticMesh* Mesh = MeshTypeData ? MeshTypeData->StaticMesh : nullptr;
		if (MeshTypeData && (!MeshTypeData->bEnabled || !Mesh))
		{
			continue;
		}

		for (int32 Index = 0; Index < Instance->ActiveParticles; ++Index)
		{
			DECLARE_PARTICLE_CONST(Particle, Instance->ParticleData + Instance->ParticleIndices[Index] * Instance->ParticleStride);
			FBox ParticleBounds;
			if (Mesh)
			{
				const FMatrix ParticleTransform = BuildMeshParticleTransform(Particle, FVector::OneVector);
				const FMatrix MeshToLocal = bLocalSpace ? ParticleTransform : ParticleTransform * WorldToLocal;
				ParticleBounds = Mesh->GetMeshData().AABB.GetWorldAABB(MeshToLocal);
			}
			else
			{
				const FVector Center = bLocalSpace ? LocalToWorld.TransformPosition(Particle.Location) : Particle.Location;
				// 빌보드는 월드 공간에서 펼쳐지므로 어느 카메라 방향과 회전에서도 쿼드를 포함한다.
				const float Radius = 0.7071068f * std::max(std::abs(Particle.Size.X), std::abs(Particle.Size.Y));
				const FVector Extent(Radius);
				ParticleBounds = FBox{ Center - Extent, Center + Extent }.GetWorldAABB(WorldToLocal);
			}
			if (bHasBounds)
			{
				Bounds.Expand(ParticleBounds);
			}
			else
			{
				Bounds = ParticleBounds;
				bHasBounds = true;
			}
		}
	}
	return Bounds;
}

void UParticleSystemComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);   // 빠지면 bCastShadow 등을 바꿔도 MarkRenderStateDirty가 안 불린다
	if (Property.Name == "Template")
	{
		const bool bShouldActivate = IsActive() || IsAutoActivate();
		InitializeSystem();
		if (bShouldActivate)
		{
			Activate();
		}
	}
	else if (Property.Name == "bAutoActivate")
	{
		if (IsAutoActivate()) Activate();
		else Deactivate();
	}
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
	int32 Count = 0;
	for (const FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Count += Instance->ActiveParticles;
		}
	}
	return Count;
}

void UParticleSystemComponent::UpdateCompletion()
{
	bWasCompleted = true;
	for (FParticleEmitterInstance*& Instance : EmitterInstances)
	{
		// 거리로 비활성화된 LOD는 완료가 아니다. 가까워지면 다시 선택될 수 있어야 한다.
		if (!Instance || !Instance->bEnabled || !Instance->CurrentLODLevel)
		{
			continue;
		}
		if (!Instance->bEmitterIsDone)
		{
			bWasCompleted = false;
			continue;
		}
		if (Instance->bKillOnCompleted)
		{
			delete Instance;
			Instance = nullptr;
		}
	}
	if (bWasCompleted)
	{
		Super::Deactivate();
		SetComponentTickEnabled(false);
	}
}

void UParticleSystemComponent::ReportEventCollision(
	const FString& InEventName,
	float InEmitterTime,
	const FVector& InLocation,
	const FVector& InDirection,
	const FVector& InVelocity,
	float InParticleTime,
	const FVector& InNormal,
	float InTime
)
{
	FParticleEventCollideData Data;
	Data.EventName = InEventName;
	Data.EmitterTime = InEmitterTime;
	Data.Location = InLocation;
	Data.Velocity = InVelocity;
	Data.ParticleTime = InParticleTime;
	Data.Direction = InDirection;
	Data.Normal = InNormal;
	Data.Time = InTime;
	CollisionEvents.Add(Data);
}

// ───────── 파티클 라이트 (Light 모듈) ─────────
// 렌더러가 읽는 것은 이번 Tick의 스냅샷(EmitterRenderData)뿐이다. 모듈 객체나 인스턴스는 보지 않는다
void UParticleSystemComponent::GatherParticleLights(TArray<FLightRenderParameters>& OutLights, int32 MaxLights) const
{
	const FMatrix LocalToWorld = GetWorldMatrix();
	for (const FDynamicEmitterDataBase* Data : EmitterRenderData)
	{
		if (!Data || !Data->bValid) continue;
		const FDynamicEmitterReplayDataBase& Base = Data->GetSource();
		if (Base.eEmitterType != DET_Sprite && Base.eEmitterType != DET_Beam && Base.eEmitterType != DET_Ribbon) continue;

		const auto& Src = static_cast<const FDynamicSpriteEmitterReplayDataBase&>(Base);
		if (Src.LightDataOffset <= 0) continue;

		for (int32 i = 0; i < Src.ActiveParticleCount; ++i)
		{
			if (static_cast<int32>(OutLights.Num()) >= MaxLights) return;

			const uint8* ParticleBase = Src.DataContainer.ParticleData + Src.ParticleStride * Src.DataContainer.ParticleIndices[i];
			const FBaseParticle& Particle = *reinterpret_cast<const FBaseParticle*>(ParticleBase);
			const FLightParticlePayload& Payload = *reinterpret_cast<const FLightParticlePayload*>(ParticleBase + Src.LightDataOffset);
			if (!Payload.bValid || Payload.Intensity <= 0.0f || Payload.Radius <= 0.0f) continue;

			FVector Color = Payload.ColorScale;
			if (Payload.bUseParticleColor)
			{
				// 알파까지 곱해 ColorOverLife로 사라질 때 빛도 같이 꺼진다
				const float A = std::clamp(Particle.Color.A, 0.0f, 1.0f);
				Color = FVector(Color.X * Particle.Color.R * A, Color.Y * Particle.Color.G * A, Color.Z * Particle.Color.B * A);
			}
			if (Color.X <= 0.0f && Color.Y <= 0.0f && Color.Z <= 0.0f) continue;

			FLightRenderParameters Light{};
			Light.Type = static_cast<uint32>(ELightType::Point);
			Light.Position = Src.bUseLocalSpace ? LocalToWorld.TransformPosition(Particle.Location) : Particle.Location;
			Light.Radius = Payload.Radius;
			Light.Color = Color;
			Light.Intensity = Payload.Intensity;
			Light.FalloffExponent = Payload.FalloffExponent;
			Light.SourceSize = 0.0f;
			Light.ShadowMapChannel = FLightRenderParameters::InvalidShadowMapChannel;
			OutLights.Add(Light);
		}
	}
}
