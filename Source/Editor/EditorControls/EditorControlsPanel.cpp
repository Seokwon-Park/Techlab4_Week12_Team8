#include "EnginePCH.h"

#include "Editor/EditorModeManager.h"
#include "Editor/EditorControls/EditorControlsPanel.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "Editor/Viewports/EditorViewportClient.h"

#include "Editor/Gizmo/Gizmo.h"
#include "GameFramework/Actor/SkeletalMeshActor.h"
#include "GameFramework/Actor/ParticleActor.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "GameFramework/Actor/DirectionalLightActor.h"
#include "GameFramework/Actor/PointLightActor.h"
#include "GameFramework/Actor/SpotLightActor.h"
#include "GameFramework/Actor/RectLightActor.h"
#include "GameFramework/Actor/SkyLightActor.h"
#include "GameFramework/Actor/DecalActor.h"
#include "GameFramework/Actor/TextRenderActor.h"
#include "GameFramework/Actor/ExponentialHeightFogActor.h"
#include "GameFramework/Character.h"

#include "Input/InputSystem.h"

#include <cmath>

// 패널 초기화 성공을 반환한다.
bool FEditorControlsPanel::Init()
{
	return true;
}

namespace
{
	// 생성할 수 있는 액터 한 종류. 클래스는 필요할 때 함수로 얻는다.
	struct FSpawnEntry
	{
		const char* Label;
		UClass* (*GetClass)();
	};

	// 콤보에서 골라 Spawn 버튼으로 생성하는 액터
	const FSpawnEntry ActorEntries[] = {
		{ "StaticMesh", &AStaticMeshActor::StaticClass },
		{ "Particle", &AParticleActor::StaticClass },
		{ "Text", &ATextRenderActor::StaticClass },
		{ "HeightFog", &AExponentialHeightFogActor::StaticClass },
		{ "Character", &ACharacter::StaticClass },
	};

	// 버튼 하나로 바로 생성하는 액터
	const FSpawnEntry LightEntries[] = {
		{ "Directional Light", &ADirectionalLightActor::StaticClass },
		{ "Spot Light", &ASpotLightActor::StaticClass },
		{ "Point Light", &APointLightActor::StaticClass },
		{ "Rect Light", &ARectLightActor::StaticClass },
		{ "Sky Light", &ASkyLightActor::StaticClass },
		{ "Decal", &ADecalActor::StaticClass },
	};

	// 순서는 UE::Widget::EWidgetMode, ECoordSystem의 값과 같다.
	const char* GizmoItems[3] = { "Location", "Rotation", "Scale" };
	const char* SpaceItems[2] = { "World", "Local" };
}

// 프레임 시간을 기억하고 Space 입력으로 기즈모 편집 모드를 순환한다.
void FEditorControlsPanel::Tick(float InDeltaTime)
{
	DeltaTime = InDeltaTime;

	if (ImGui::GetIO().WantTextInput)
		return;

	if (Gizmo && FInputSystem::IsKeyPressed(EKeyCode::Space))
	{
		// 편집 모드는 레벨 에디터의 모드 툴이 갖는다.
		FEditorModeTools& ModeTools = GLevelEditorModeTools();
		const int NextMode = (static_cast<int>(ModeTools.GetWidgetMode()) + 1) % 3;
		ModeTools.SetWidgetMode(static_cast<UE::Widget::EWidgetMode>(NextMode));
	}
}

// 패널 전체를 그린다. 섹션의 순서는 여기서 정한다.
void FEditorControlsPanel::OnRender()
{
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGui::Begin("Editor Controls");
	ImGui::Spacing();

	// 오른쪽 위에 FPS와 프레임 시간을 적는다.
	char FpsText[64];
	std::snprintf(FpsText, sizeof(FpsText), "FPS %.1f   %.1f ms", 1.0f / DeltaTime, DeltaTime * 1000.0f);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(FpsText).x);
	ImGui::TextDisabled("%s", FpsText);

	if (World)
	{
		DrawSpawnSection();
		DrawGizmoSection();
		DrawCameraProperties();
		DrawSkeletalMeshSection();
	}

	ImGui::End();
}

// 액터 종류를 골라 생성하는 줄과, 라이트와 데칼을 바로 생성하는 버튼 줄을 그린다.
void FEditorControlsPanel::DrawSpawnSection()
{
	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Actor Spawn");

	// 한 줄에 콤보, Spawn 버튼, 액터 수를 놓는다. 콤보가 남는 너비를 차지한다.
	char CountText[32];
	std::snprintf(CountText, sizeof(CountText), "%d Actors", World->GetActorNum());

	const float SpawnButtonWidth = 70.0f;
	const float Spacing = ImGui::GetStyle().ItemSpacing.x;
	const float ComboWidth = ImGui::GetContentRegionAvail().x - SpawnButtonWidth
		- ImGui::CalcTextSize(CountText).x - Spacing * 2.0f;

	ImGui::SetNextItemWidth(ComboWidth);
	if (ImGui::BeginCombo("##ActorType", ActorEntries[SelectedActorIndex].Label))
	{
		for (int32 Index = 0; Index < IM_ARRAYSIZE(ActorEntries); ++Index)
		{
			if (ImGui::Selectable(ActorEntries[Index].Label, Index == SelectedActorIndex))
				SelectedActorIndex = Index;
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("Spawn", ImVec2(SpawnButtonWidth, 0)))
		World->SpawnActor(ActorEntries[SelectedActorIndex].GetClass());

	ImGui::SameLine();
	ImGui::TextDisabled("%s", CountText);

	ImGui::Dummy(ImVec2(0.0f, SubsectionGap));
	ImGui::SeparatorText("Light Spawn");
	for (int32 Index = 0; Index < IM_ARRAYSIZE(LightEntries); ++Index)
	{
		if (Index > 0) ImGui::SameLine();
		if (ImGui::Button(LightEntries[Index].Label))
			World->SpawnActor(LightEntries[Index].GetClass());
	}
}

// 기즈모의 편집 모드와 좌표계를 고르는 콤보를 그린다. 값은 레벨 에디터의 모드 툴이 갖는다.
void FEditorControlsPanel::DrawGizmoSection()
{
	int32 GizmoSelectedIndex = 0;
	int32 SpaceSelectedIndex = 0;

	// Gizmo가 없는 앱(Benchmark 등)에서는 이 섹션을 통째로 건너뛴다.
	if (Gizmo)
	{
		ImGui::Dummy(ImVec2(0.0f, SectionGap));
		ImGui::SeparatorText("Gizmo");
	}

	if (Gizmo && ImGui::BeginTable("GizmoControls", 2))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Mode");

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);

		FEditorModeTools& ModeTools = GLevelEditorModeTools();
		GizmoSelectedIndex = static_cast<int32>(ModeTools.GetWidgetMode());
		if (ImGui::Combo("##GizmoMode", &GizmoSelectedIndex, GizmoItems, IM_ARRAYSIZE(GizmoItems)))
		{
			ModeTools.SetWidgetMode(static_cast<UE::Widget::EWidgetMode>(GizmoSelectedIndex));
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Space");

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);

		// SpaceItems의 순서는 ECoordSystem의 값(World가 0, Local이 1)과 같다.
		SpaceSelectedIndex = static_cast<int32>(ModeTools.GetCoordSystem());
		if (ImGui::Combo("##GizmoSpace", &SpaceSelectedIndex, SpaceItems, IM_ARRAYSIZE(SpaceItems)))
		{
			ModeTools.SetCoordSystem(static_cast<ECoordSystem>(SpaceSelectedIndex));
		}

		ImGui::EndTable();
	}
}

// GPU 스키닝 토글과 본 테스트용 도구를 그린다.
void FEditorControlsPanel::DrawSkeletalMeshSection()
{
	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Skeletal Mesh");
	if (ImGui::Button("Test Bone Stick"))
		World->SpawnActor(ASkeletalMeshActor::StaticClass());

	bool bGPU = USkinnedMeshComponent::bUseGPUSkinning;    // 지역 복사본에 체크
	if (ImGui::Checkbox("GPU Skinning", &bGPU))
		USkinnedMeshComponent::SetGPUSkinningEnabled(bGPU);

	static float Bone1Pitch = 0.0f;
	if (ImGui::SliderFloat("Bone_1 Pitch", &Bone1Pitch, -90.0f, 90.0f))
	{
		for (AActor* Actor : World->GetPersistentLevel()->GetActors())
		{
			ASkeletalMeshActor* SkelActor = Cast<ASkeletalMeshActor>(Actor);
			if (!SkelActor) continue;
			USkeletalMeshComponent* Comp = SkelActor->GetSkeletalMeshComponent();
			const int32 Bone = Comp->FindBoneIndex("Bone_1");
			if (Bone < 0) continue;

			FTransform Local = Comp->GetBoneLocalTransform(Bone);
			Local.Rotation = FRotator(Bone1Pitch, 0.0f, 0.0f).Quaternion();
			Comp->SetBoneLocalTransform(Bone, Local);
			Comp->RefreshBoneTransformsNow();   // 에디터 월드에서 Tick이 안 돌아도 바로 반영
		}
	}
}

// 실제 View 속성을 편집하며 패널 이동 후에도 마지막 View를 유지하고 변경된 값만 반영한다.
void FEditorControlsPanel::DrawCameraProperties()
{
	ImGui::Dummy(ImVec2(0.0f, SectionGap));
	ImGui::SeparatorText("Viewport");

	UCameraComponent* CamCom = World && World->GetMainCamera() ? World->GetMainCamera()->GetCameraComponent() : nullptr;
	if (!CamCom) return;

	FEditorViewportClient* ActiveClient = ViewportsPanel ? ViewportsPanel->GetViewportClient(ViewportsPanel->GetActiveViewIndex()) : nullptr;

	bool bOrthogonal = CamCom->GetIsOrthogonal();

	ImGui::Dummy(ImVec2(0.0f, SubsectionGap));
	ImGui::TextDisabled("Projection");

	// 카메라 투영 설정 표
	if (ImGui::BeginTable("Camera Projection", 2))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		// FOV 또는 Ortho Width
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::Text(bOrthogonal ? "Ortho Width" : "FOV");

		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(-1.0f);

		if (bOrthogonal)
		{
			float OrthoWidth = CamCom->GetOrthoWidth();
			if (ImGui::DragFloat("##OrthoWidth", &OrthoWidth, 0.1f) && std::isfinite(OrthoWidth))
			{
				OrthoWidth = FMath::Clamp(OrthoWidth, 0.01f, 1000000.0f);
				CamCom->SetOrthoWidth(OrthoWidth);
				if (ActiveClient) ActiveClient->SetOrthoWidth(OrthoWidth);
			}
		}
		else
		{
			float FOV = CamCom->GetFieldOfView();
			if (ImGui::DragFloat("##FOV", &FOV, 0.1f) && std::isfinite(FOV))
			{
				FOV = FMath::Clamp(FOV, 1.0f, 179.0f);
				CamCom->SetFieldOfView(FOV);
				if (ActiveClient) ActiveClient->SetViewFOV(FOV);
			}
		}

		// 가까운 평면과 먼 평면
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Near");

		ImGui::TableSetColumnIndex(1);
		float Near = CamCom->GetNearZ();

		ImGui::SetNextItemWidth(-1.0f);
		bool Changed = ImGui::InputFloat("##Near", &Near);

		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Far");

		ImGui::TableSetColumnIndex(1);
		float Far = CamCom->GetFarZ();
		ImGui::SetNextItemWidth(-1.0f);
		Changed |= ImGui::InputFloat("##Far", &Far);

		if (Changed && std::isfinite(Near) && std::isfinite(Far) && Near > 0 && Far > Near)
		{
			CamCom->SetNearZ(Near);
			CamCom->SetFarZ(Far);
		}
		ImGui::EndTable();
	}

	// 카메라 변환 설정 표
	ImGui::Dummy(ImVec2(0.0f, SubsectionGap));
	ImGui::TextDisabled("Transform");

	bool bLocationChanged = false;
	bool bRotationChanged = false;

	if (ImGui::BeginTable("Camera Transform", 2))
	{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();

		// 카메라 위치
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Location");

		ImGui::TableSetColumnIndex(1);
		float Available = ImGui::GetContentRegionAvail().x;
		float ItemWidth = (Available - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;

		FVector Location = CamCom->GetRelativeLocation();

		ImGui::SetNextItemWidth(ItemWidth);
		bLocationChanged |= ImGui::DragFloat("##LocationX", &Location.X, 0.1f);

		ImGui::SameLine();

		ImGui::SetNextItemWidth(ItemWidth);
		bLocationChanged |= ImGui::DragFloat("##LocationY", &Location.Y, 0.1f);

		ImGui::SameLine();

		ImGui::SetNextItemWidth(ItemWidth);
		bLocationChanged |= ImGui::DragFloat("##LocationZ", &Location.Z, 0.1f);

		if (bLocationChanged && std::isfinite(Location.X) && std::isfinite(Location.Y) && std::isfinite(Location.Z))
		{
			CamCom->SetRelativeLocation(Location);
			if (ActiveClient) ActiveClient->SetViewLocation(Location);
		}

		ImGui::TableNextRow();

		// 카메라 회전
		ImGui::TableSetColumnIndex(0);
		ImGui::Text("Rotation");

		ImGui::TableSetColumnIndex(1);
		FRotator Rotation = CamCom->GetRelativeRotation();

		ImGui::SetNextItemWidth(ItemWidth);
		bRotationChanged |= ImGui::DragFloat("##Pitch", &Rotation.Pitch, 0.1f);

		ImGui::SameLine();

		ImGui::SetNextItemWidth(ItemWidth);
		bRotationChanged |= ImGui::DragFloat("##Yaw", &Rotation.Yaw, 0.1f);

		ImGui::SameLine();

		ImGui::SetNextItemWidth(ItemWidth);
		ImGui::BeginDisabled(!bOrthogonal);
		bRotationChanged |= ImGui::DragFloat("##Roll", &Rotation.Roll, 0.1f);
		ImGui::EndDisabled();

		if (bRotationChanged && std::isfinite(Rotation.Pitch) && std::isfinite(Rotation.Yaw) && std::isfinite(Rotation.Roll))
		{
			CamCom->SetRelativeRotation(Rotation);
			if (ActiveClient) ActiveClient->SetViewRotation(Rotation);
		}
		ImGui::EndTable();
	}
}
