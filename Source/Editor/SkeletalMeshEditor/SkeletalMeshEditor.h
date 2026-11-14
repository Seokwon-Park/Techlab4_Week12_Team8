#pragma once

#include "Editor/SkeletalPreviewEditor/SkeletalPreviewEditor.h"
#include "Editor/Gizmo/Gizmo.h"
#include "Editor/EditorModeManager.h"
#include "Editor/Viewports/EditorViewportClient.h"


class FSkeletalMeshEditor : public FSkeletalPreviewEditor
{
public:
	FSkeletalMeshEditor(UEditorEngine& InEditorEngine, FEditorManager& InEditorManager,
		const FString& InAssetPath, USkeletalMesh& InMesh)
		: FSkeletalPreviewEditor(InEditorEngine, InEditorManager, InAssetPath)
	{
		Mesh = &InMesh;
	}

	const char* GetEditorTypeName() const override { return "SkeletalMesh"; }
	const FGizmo* GetPreviewGizmo() const override { return BoneGizmo.GetTarget() ? &BoneGizmo : nullptr; }

	// 선택한 본의 프리뷰 포즈를 부모 기준 로컬 변환으로 조회·수정합니다.
	bool GetSelectedBoneLocalTransform(FTransform& OutTransform) const;
	void SetSelectedBoneLocalTransform(const FTransform& Transform);
	void ResetPose();
	void ResetBone(int32 BoneIndex);

	void SetGizmoMode(UE::Widget::EWidgetMode Mode);
	void SetGizmoSpace(ECoordSystem Space);
	UE::Widget::EWidgetMode  GetGizmoMode() const { return ModeTools.GetWidgetMode(); }
	ECoordSystem GetGizmoSpace() const { return ModeTools.GetCoordSystem(); }
	bool IsGizmoDragging() const { return BoneGizmo.IsUsing(); }

protected:
	// 이 에디터에 연결된 탭을 생성합니다.
	TUniquePtr<IEditorTab> CreateTab() override;
	bool InitializeResources() override;     // InitializePreview() 후 BoneGizmoTarget 생성
	void ShutdownResources() override;       // 기즈모 정리 후 Super

	void TickAfterWorld(float DeltaTime) override;
	bool IsViewportInteractionBusy() const override { return BoneGizmo.IsUsing(); }
	void OnBoneSelected() override { BoneGizmo.EndDrag(); SyncBoneGizmo(); }
	void CancelViewportInteraction() override { BoneGizmo.EndDrag(); }

private:
	void SyncBoneGizmo();
	bool UpdateBoneGizmo();
	void ApplyBoneGizmoTransform();
	FQuat GetBoneWorldRotation(int32 BoneIndex) const;

	USceneComponent* BoneGizmoTarget = nullptr;
	// 이 에디터의 편집 모드와 좌표계. 에셋 에디터는 레벨 에디터와 따로 하나씩 갖는다. BoneGizmo보다 먼저 선언해야 한다.
	FEditorModeTools ModeTools;
	FGizmo BoneGizmo{ &ModeTools };
	FTransform DragStartBoneTransform;
	FMatrix DragParentInverse;
	FQuat DragParentRotation;
};
