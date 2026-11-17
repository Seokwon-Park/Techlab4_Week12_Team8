#include "EnginePCH.h"
#include "ParticleSystemEditor.h"

FParticleSystemEditor::FParticleSystemEditor(UEditorEngine& InEditorEngine, FEditorManager& InEditorManager,
	const FString& InAssetPath, UParticleSystem* InSystem)
	: FAssetEditor(InEditorEngine, InEditorManager, InAssetPath)
	, System(InSystem)
{
}

// 헤더에 override로 선언한 것만큼 빈 본문을 둔다
void FParticleSystemEditor::Tick(float DeltaTime) { FAssetEditor::Tick(DeltaTime); }
void FParticleSystemEditor::OnRenderPreview() {}
bool FParticleSystemEditor::ConfirmClose() { return true; }
void FParticleSystemEditor::OnRenderLine(FLineBatcher&) {}