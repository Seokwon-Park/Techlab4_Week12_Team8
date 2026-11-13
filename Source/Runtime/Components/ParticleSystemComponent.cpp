#include "EnginePCH.h"
#include "ParticleSystemComponent.h"

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
}

void UParticleSystemComponent::OnUnregister()
{
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return nullptr;
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
}

int32 UParticleSystemComponent::GetNumActiveParticles() const
{
	return int32();
}
