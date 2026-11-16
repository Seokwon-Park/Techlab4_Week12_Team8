#include "EnginePCH.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "Editor/Viewports/LevelEditorViewport.h"
#include "Engine/GameViewportClient.h"

#include "Core/StatOverlay.h"
#include "Core/Stats/LightweightStats.h"
#include "Render/RenderCommand.h"

#include <algorithm>
#include <cassert>
#include <format>

#include "Core/Stats/EditorStats.h"
#include "UObject/UObjectIterator.h"
#include "Render/GPUStats.h"
#include "Render/SkinningStats.h"
#include "Render/SkeletalMeshObject.h"
#include "Components/SkinnedMeshComponent.h"
#include "Editor/EditorUI/EditorIcons.h"
#include "Editor/EditorModeManager.h"
#include "Input/InputSystem.h"


namespace {
constexpr float SplitterThickness = 6.0f;
constexpr ImU32 SplitterColor = IM_COL32(55, 55, 55, 230);
constexpr ImU32 SplitterHoverColor = IM_COL32(255, 192, 0, 255);
constexpr const char *ViewportTypeLabels[] = {
		"Perspective", "Top", "Front", "Right"};

// Stat Overlay
constexpr float StatOverlayMargin = 8.0f;
constexpr float StatOverlayPadding = 6.0f;
constexpr ImU32 StatOverlayBackgroundColor = IM_COL32(0, 0, 0, 140);
constexpr ImU32 TitleColor = IM_COL32(255, 210, 60, 255);
constexpr ImU32 ValueColor = IM_COL32(235, 235, 235, 255);
} // namespace

// 네 View의 렌더 타깃을 최소 크기로 초기화한다.
bool FViewportsPanel::Init() {
	for (int32 i = 0; i < 4; ++i) {
		ViewportClients[i] = MakeUnique<FLevelEditorViewportClient>();
		ViewportClients[i]->SetViewIndex(i);

		// 뷰포트를 만들어 클라이언트와 서로 연결하고, 타깃을 최소 크기로 만들어 둔다.
		Viewports[i] = MakeUnique<FViewport>(ViewportClients[i].get());
		ViewportClients[i]->Viewport = Viewports[i].get();
		Viewports[i]->UpdateViewportRHI(false, 1, 1);
	}

	// 초기 시점 설정
	ViewportClients[0]->SetViewportType(ELevelViewportType::Perspective);
	ViewportClients[1]->SetViewportType(ELevelViewportType::Top);
	ViewportClients[2]->SetViewportType(ELevelViewportType::Front);
	ViewportClients[3]->SetViewportType(ELevelViewportType::Right);

	return true;
}

// 패널의 프레임 갱신 인터페이스이며 별도 계산은 하지 않는다.
// UE5처럼 뷰포트 위에서 Q/W/E/R로 Select/Translate/Rotate/Scale 도구를 고른다.
// 오른쪽 버튼으로 카메라를 움직이는 중(W/E/Q가 이동 키)이거나 글자 입력 중, 뷰포트에서 게임 중이면 받지 않는다.
void FViewportsPanel::Tick(float DeltaTime) {
	(void)DeltaTime;
	if (!IsHovered() || ImGui::GetIO().WantTextInput || GameViewportClient ||
		FInputSystem::IsMouseDown(EMouseButton::Right))
		return;

	FEditorModeTools& ModeTools = GLevelEditorModeTools();
	if (FInputSystem::IsKeyPressed(EKeyCode::Q)) ModeTools.SetWidgetMode(UE::Widget::WM_None);
	else if (FInputSystem::IsKeyPressed(EKeyCode::W)) ModeTools.SetWidgetMode(UE::Widget::WM_Translate);
	else if (FInputSystem::IsKeyPressed(EKeyCode::E)) ModeTools.SetWidgetMode(UE::Widget::WM_Rotate);
	else if (FInputSystem::IsKeyPressed(EKeyCode::R)) ModeTools.SetWidgetMode(UE::Widget::WM_Scale);
}

// Core Rect에 맞춰 View 활성 상태와 타깃 크기를 갱신한다.
void FViewportsPanel::SetView(const int32 ViewIndex, const FRect &Rect,
			const bool bActive) {
	assert(ViewIndex >= 0 && ViewIndex < 4);
	FEditorViewportClient *Client = ViewportClients[ViewIndex].get();
	if (!Client)
		return;

	Client->SetRect(Rect);
	const bool bSlotActive = bActive && Rect.Width > 0.0f && Rect.Height > 0.0f;
	Client->SetActive(bSlotActive);
	if (!bSlotActive)
		return;

	// 크기가 같으면 뷰포트가 아무것도 하지 않는다. 바뀌면 타깃을 다시 만들고 클라이언트에 알린다.
	const uint32 Width = static_cast<uint32>(std::max(1.0f, Rect.Width));
	const uint32 Height = static_cast<uint32>(std::max(1.0f, Rect.Height));
	Viewports[ViewIndex]->UpdateViewportRHI(false, Width, Height);
}

// 마우스 위치에서 패널 원점을 빼 로컬 좌표로 바꾼다.
FVector2 FViewportsPanel::GetLocalMousePosition() const {
	POINT Cursor{};
	const ImVec2 Mouse = ::GetCursorPos(&Cursor)
		? ImVec2(static_cast<float>(Cursor.x), static_cast<float>(Cursor.y))
		: ImGui::GetMousePos();
	return {Mouse.x - ContentOrigin.x, Mouse.y - ContentOrigin.y};
}

// 엔진 입력 처리는 ImGui NewFrame보다 먼저 실행되므로 현재 OS 커서로 버튼
// 영역을 검사한다.
bool FViewportsPanel::IsHovered() const {
	// 뷰포트 툴바 위에 있거나 툴바 메뉴가 열려 있으면 뷰포트를 누른 것으로 보지 않는다.
	if (bToolbarHovered)
		return false;
	if (bStatResetButtonVisible && FStatOverlay::IsAnyEnabled()) {
		POINT Cursor{};
		if (::GetCursorPos(&Cursor) && Cursor.x >= StatResetButtonMin.x &&
				Cursor.x < StatResetButtonMax.x && Cursor.y >= StatResetButtonMin.y &&
				Cursor.y < StatResetButtonMax.y)
			return false;
	}
	const FVector2 Mouse = GetLocalMousePosition();
	return bHovered && Mouse.X >= 0.0f && Mouse.Y >= 0.0f &&
		Mouse.X < ContentSize.x && Mouse.Y < ContentSize.y;
}

// 누적 가로 Splitter 이동량을 반환하고 초기화한다.
float FViewportsPanel::ConsumeHorizontalDrag() {
	const float Result = PendingHorizontalDrag;
	PendingHorizontalDrag = 0.0f;
	return Result;
}

// 누적 세로 Splitter 이동량을 반환하고 초기화한다.
float FViewportsPanel::ConsumeVerticalDrag() {
	const float Result = PendingVerticalDrag;
	PendingVerticalDrag = 0.0f;
	return Result;
}

// 대기 Layout 요청을 한 번 반환하고 플래그를 지운다.
bool FViewportsPanel::ConsumeLayoutRequest(ELayoutMode &OutMode,
			int32 &OutSingleViewIndex) {
	if (!bHasLayoutRequest)
		return false;
	OutMode = RequestedLayoutMode;
	OutSingleViewIndex = RequestedSingleViewIndex;
	CurrentLayoutMode = RequestedLayoutMode;
	CurrentSingleViewIndex = RequestedSingleViewIndex;
	bHasLayoutRequest = false;
	return true;
}



// View Texture와 Splitter·Layout·Preset UI를 그리고 요청을 기록한다.
void FViewportsPanel::OnRender() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
	EditorPanelUI::BeginPanel("Viewports", nullptr,
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoTitleBar);

	ContentOrigin = ImGui::GetCursorScreenPos();
	ContentSize = ImGui::GetContentRegionAvail();
	ContentSize.x = std::max(1.0f, ContentSize.x);
	ContentSize.y = std::max(1.0f, ContentSize.y);
	bHovered = ImGui::IsWindowHovered();
	bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	bStatResetButtonVisible = false;

	// 전체 캔버스를 한 번 확보한 뒤 각 렌더 타깃을 창 DrawList에 직접 그린다.
	// Image 항목 네 개를 따로 배치하면 ImGui 레이아웃과 클리핑 상태가 삽입 순서에
	// 영향을 받아, Core가 올바른 사각형을 줘도 아래쪽 행이 잘릴 수 있다.
	ImGui::Dummy(ContentSize);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(ContentOrigin,
		{ContentOrigin.x + ContentSize.x, ContentOrigin.y + ContentSize.y}, true);
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex)
	{
		FEditorViewportClient* Client = ViewportClients[ViewIndex].get();
		if (!Client || !Client->IsActive())
			continue;

		const FRect &Rect = Client->GetRect();
		const ImVec2 ViewMin{ContentOrigin.x + Rect.X,
			ContentOrigin.y + Rect.Y};
		const ImVec2 ViewMax{ViewMin.x + Rect.Width,
			ViewMin.y + Rect.Height};

		const bool bGameViewport = GameViewportClient && ViewIndex == GameViewportIndex;
		if (bGameViewport)
		{
			GameViewportClient->SetRect(FRect{
				ViewMin.x,
				ViewMin.y,
				Rect.Width,
				Rect.Height
				});
		}


		if (ViewIndex == 0 && bShowNoCamera && !bGameViewport)
		{
			// 메인 원근 뷰포트 영역을 어두운 회색으로 채움
			DrawList->AddRectFilled(ViewMin, ViewMax, IM_COL32(45, 45, 48, 255));
			const char* WarningText = "No Camera";
			ImVec2 TextSize = ImGui::CalcTextSize(WarningText);
			ImVec2 CenterPos{
				ViewMin.x + (Rect.Width - TextSize.x) * 0.5f,
				ViewMin.y + (Rect.Height - TextSize.y) * 0.5f
			};
			DrawList->AddText(CenterPos, IM_COL32(220, 220, 220, 255), WarningText);
			continue;
		}

		FTexture2D* DisplayTarget = bGameViewport
			? (GameViewportClient->Viewport ? GameViewportClient->Viewport->GetRenderTargetTexture() : nullptr)
			: Viewports[ViewIndex]->GetRenderTargetTexture();
		if (!DisplayTarget || !DisplayTarget->GetSRV())
			continue;

		DrawList->AddImage(DisplayTarget->GetSRV(), ViewMin, ViewMax);

		// 마우스 클릭 시 활성 뷰포트 설정
		const bool bViewHovered = bHovered && ImGui::IsMouseHoveringRect(ViewMin, ViewMax);
		if (bViewHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
				ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
				ImGui::IsMouseClicked(ImGuiMouseButton_Middle))) {
			ActiveViewIndex = ViewIndex;
			ImGui::SetWindowFocus();
			bFocused = true;
		}
	}
	DrawList->PopClipRect();

	if (CurrentLayoutMode == ELayoutMode::QuadSplit && ViewportClients[0] && ViewportClients[1] && ViewportClients[2]) {
		const FRect &Rect0 = ViewportClients[0]->GetRect();
		const FRect &Rect1 = ViewportClients[1]->GetRect();
		const FRect &Rect2 = ViewportClients[2]->GetRect();
		// 분할선 배치 계산
		const float SplitX =
				(Rect0.X + Rect0.Width + Rect1.X) * 0.5f;
		const float SplitY =
				(Rect0.Y + Rect0.Height + Rect2.Y) * 0.5f;

		const ImVec2 VerticalMin{
				ContentOrigin.x + SplitX - SplitterThickness * 0.5f, ContentOrigin.y};
		const ImVec2 VerticalMax{VerticalMin.x + SplitterThickness,
			ContentOrigin.y + ContentSize.y};
		const ImVec2 HorizontalMin{ContentOrigin.x, ContentOrigin.y + SplitY -
			SplitterThickness * 0.5f};
		const ImVec2 HorizontalMax{ContentOrigin.x + ContentSize.x,
			HorizontalMin.y + SplitterThickness};

		ImGui::SetCursorScreenPos(VerticalMin);
		ImGui::InvisibleButton("##MultipleViewportsHorizontalSplitter",
													 {SplitterThickness, ContentSize.y});
		ImGui::SetCursorScreenPos(HorizontalMin);
		ImGui::InvisibleButton("##MultipleViewportsVerticalSplitter",
													 {ContentSize.x, SplitterThickness});

		const bool bVerticalHovered =
				bHovered && ImGui::IsMouseHoveringRect(VerticalMin, VerticalMax);
		const bool bHorizontalHovered =
				bHovered && ImGui::IsMouseHoveringRect(HorizontalMin, HorizontalMax);
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			bDraggingVerticalSplitter = bVerticalHovered;
			bDraggingHorizontalSplitter = bHorizontalHovered;
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			bDraggingVerticalSplitter = false;
			bDraggingHorizontalSplitter = false;
		}

		const bool bVerticalHighlighted =
				bDraggingVerticalSplitter || bVerticalHovered;
		const bool bHorizontalHighlighted =
				bDraggingHorizontalSplitter || bHorizontalHovered;
		DrawList->AddRectFilled(VerticalMin, VerticalMax,
			bVerticalHighlighted ? SplitterHoverColor
			: SplitterColor);
		DrawList->AddRectFilled(HorizontalMin, HorizontalMax,
			bHorizontalHighlighted ? SplitterHoverColor
			: SplitterColor);

		if (bDraggingVerticalSplitter)
			PendingHorizontalDrag += ImGui::GetIO().MouseDelta.x;
		if (bDraggingHorizontalSplitter)
			PendingVerticalDrag += ImGui::GetIO().MouseDelta.y;

		if (bVerticalHighlighted && bHorizontalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
		else if (bVerticalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		else if (bHorizontalHighlighted)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
	} else {
		bDraggingVerticalSplitter = false;
		bDraggingHorizontalSplitter = false;
	}

	// 뷰마다 위쪽에 UE5식 뷰포트 툴바를 그린다.
	bToolbarHovered = false;
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex) {
		FEditorViewportClient *Client = ViewportClients[ViewIndex].get();
		if (!Client || !Client->IsActive())
			continue;
		const FRect &Rect = Client->GetRect();
		const ImVec2 ViewMin{ContentOrigin.x + Rect.X, ContentOrigin.y + Rect.Y};
		DrawViewportToolbar(ViewIndex, Client, ViewMin, {ViewMin.x + Rect.Width, ViewMin.y + Rect.Height});
	}
	// 툴바 메뉴가 열려 있으면 뷰포트 클릭(피킹)을 막는다.
	if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
		bToolbarHovered = true;

	// 마지막으로 선택된 뷰포트만 오버레이
	for (int32 ViewIndex = 0; ViewIndex < 4; ++ViewIndex) {
		if (ViewIndex != ActiveViewIndex)
			continue;
		FEditorViewportClient *Client = ViewportClients[ViewIndex].get();
		if (!Client || !Client->IsActive() || Client->IsLightCulling())
			continue;
		DrawStatOverlay(DrawList, { ContentOrigin.x + Client->GetRect().X, ContentOrigin.y + Client->GetRect().Y + GetToolbarHeight() }, Client);
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

// 항목별 줄을 모아 한 번에 배경과 텍스트를 그린다.
void FViewportsPanel::DrawStatOverlay(ImDrawList* DrawList, const ImVec2& ViewMin, FEditorViewportClient* Client) const
{
	if (!DrawList || !FStatOverlay::IsAnyEnabled())
		return;

	constexpr float BytesPerMegabyte = 1024.0f * 1024.0f;

	// 제목은 UE처럼 노란색, 값은 흰색으로 구분한다.
	struct FStatLine {
		FString Text;
		ImU32 Color;
	};
	TArray<FStatLine> Lines;

	if (FStatOverlay::IsEnabled(EStatFlags::FPS)) {
		Lines.Add({"FPS", TitleColor});
		Lines.Add(
				{std::format("  {:.1f} fps", FStatOverlay::GetFPS()), ValueColor});
		Lines.Add({std::format("  {:.2f} ms", FStatOverlay::GetFrameTimeMs()),
			ValueColor});
	}

	if (FStatOverlay::IsEnabled(EStatFlags::Memory)) {
		Lines.Add({"Memory", TitleColor});
		Lines.Add({std::format("  Object  {:.2f} MB ({} allocs)",
			static_cast<double>(
			FStatOverlay::GetObjectAllocationBytes()) /
			BytesPerMegabyte,
			FStatOverlay::GetObjectAllocationCount()),
			ValueColor});
		Lines.Add({std::format("  Process {:.2f} MB",
			static_cast<double>(
			FStatOverlay::GetProcessWorkingSetBytes()) /
			BytesPerMegabyte),
			ValueColor});
	}

	struct FProfileStatLine {
		FString Text;
		ImU32 Color;
		FString Values[3];
	};
	TArray<FProfileStatLine> ProfileLines;
	if (FStatOverlay::IsEnabled(EStatFlags::Profile)) {
		ProfileLines.Add({"CPU Profile (ms)", TitleColor, {"Last", "Avg", "Max"}});

		for (const auto &[Name, Data] : FStatRegistry::GetAll()) {
			if (TStatId{Name} == EditorStats::STAT_PickingTime)
				continue;

			FProfileStatLine Line{"  " + FString(Name), ValueColor};
			Line.Values[0] = std::format("{:.2f}", Data.GetLastMs());
			Line.Values[1] = std::format("{:.2f}", Data.GetRecentAverageMs());
			Line.Values[2] = std::format("{:.2f}", Data.GetMaxMs());
			ProfileLines.Add(Line);
		}
	}

	if (const FCycleStatData *PickingData =
					FStatRegistry::Find(EditorStats::STAT_PickingTime)) {
		ProfileLines.Add({"Picking", TitleColor});
		FProfileStatLine Line{"  Time (ms)", ValueColor};
		Line.Values[0] = std::format("Last: {:.2f}", PickingData->GetLastMs());
		Line.Values[1] = std::format("Attempts: {}", PickingData->CallCount);
		Line.Values[2] = std::format("Acc.: {:.2f}", PickingData->GetTotalMs());
		ProfileLines.Add(Line);
	}

FSceneRenderData& RenderData = Client->GetRenderData();
const FRenderStats& Stats = RenderData.RenderStats;
	if (FStatOverlay::IsEnabled(EStatFlags::Light)) {
		Lines.Add({ "Light", TitleColor });
		Lines.Add({ std::format("  Total Lights {} ", Stats.DirectionalCount + Stats.PointCount + Stats.SpotCount), ValueColor });
		Lines.Add({ std::format("  Directional {} ", Stats.DirectionalCount), ValueColor });
		Lines.Add({ std::format("  Point {} ", Stats.PointCount), ValueColor });
		Lines.Add({ std::format("  Spot {} ", Stats.SpotCount), ValueColor });

		Lines.Add({ "Shadow Atlas", TitleColor });
		Lines.Add({ std::format("  Atlas Capacity  {:.2f} MB",
			static_cast<double>(Stats.AtlasCapacity) /
			BytesPerMegabyte),
			ValueColor });
		Lines.Add({ std::format("  Shadow Tiles  {:.2f} MB",
			static_cast<double>(Stats.ShadowTiles) /
			BytesPerMegabyte),
			ValueColor });
		Lines.Add({ std::format("  Tile Usage  {:.1f} %",
			static_cast<double>(Stats.ShadowTiles) /
			static_cast<double>(Stats.AtlasCapacity) * 100),
			ValueColor });
		Lines.Add({ std::format("  Shadow Views  Redrawn {} / Cached {}",
			Stats.ShadowViewsRedrawn, Stats.ShadowViewsCached),
			ValueColor });
		Lines.Add({ std::format("  ShadowLightsSkippedOutsideView {}",
			Stats.ShadowLightsSkippedOutsideView),
			ValueColor });
		Lines.Add({ std::format("  ShadowLightsSkippedNoReceiver {}",
			Stats.ShadowLightsSkippedNoReceiver),
			ValueColor });
		Lines.Add({ std::format("  ShadowLightsSkippedNoCaster {}",
			Stats.ShadowLightsSkippedNoCaster),
			ValueColor });
		Lines.Add({ std::format("  ShadowCastersSkippedSmall {}",
			Stats.ShadowCastersSkippedSmall),
			ValueColor });

		if (RenderData.CascadeShadowCount > 0)
		{
			Lines.Add({ "Cascaded Shadow Map", TitleColor });
			Lines.Add({ std::format("  Cascades {}", RenderData.CascadeShadowCount), ValueColor });

			// 첫 번째 Directional의 그림자 뷰들 = 캐스케이드들
			const uint32 DirOffset = RenderData.GetDirectionalLightOffset();
			const bool bHasDirShadow = RenderData.GetDirectionalLightCount() > 0 &&
				DirOffset < static_cast<uint32>(RenderData.VisibleLightShadowViews.Num()) &&
				RenderData.VisibleLightShadowViews[DirOffset] != FSceneRenderData::InvalidShadowView;

			float PrevDistance = 0.0f;
			for (uint32 c = 0; c < RenderData.CascadeShadowCount; ++c)
			{
				const float Far = RenderData.CascadeShadowDistances[c];
				FString Texel = "-";
				if (bHasDirShadow)
				{
					const uint32 ShadowIdx = RenderData.VisibleLightShadowViews[DirOffset] + c;
					if (ShadowIdx < uint32(RenderData.LightShadows.Num()))
						Texel = std::format("{:.3f}", RenderData.LightShadows[ShadowIdx].WorldUnitsPerTexel);
				}
				Lines.Add({ std::format("  C{}  {:.1f} ~ {:.1f}   texel {}", c, PrevDistance, Far, Texel), ValueColor });
				PrevDistance = Far;
			}
			Lines.Add({ std::format("  Cascade Views  Redrawn {} / Cached {}",
				Stats.CascadeViewsRedrawn, Stats.CascadeViewsCached), ValueColor });
		}
	}
	if (FStatOverlay::IsEnabled(EStatFlags::Decal)) {
		Lines.Add({ "Decal", TitleColor });
		Lines.Add({ std::format("  Decals {}", Stats.DecalCount), ValueColor });
		Lines.Add({ std::format("  Decals Rendered {}", Stats.DecalsRendered), ValueColor });
		Lines.Add({ std::format("  Decal Draw Calls {}", Stats.DecalDrawCalls), ValueColor });
		Lines.Add({ std::format("  Decal Items Tested {}", Stats.DecalItemsTested), ValueColor });
		Lines.Add({ std::format("  Decal Items Culled {}", Stats.DecalItemsTested - Stats.DecalDrawCalls), ValueColor });
		Lines.Add({ std::format("  Decal Broad Tested {}", Stats.DecalBroadTests), ValueColor });
		Lines.Add({ std::format("  Decal Narrow Tested {}", Stats.DecalNarrowTested), ValueColor });
		Lines.Add({ std::format("  Decal OBBTree Tested {}", Stats.DecalOBBTreeTested), ValueColor });

		const FCycleStatData* DecalGPU = FStatRegistry::FindGPU(GPUStats::STAT_GPU_Decal);
		Lines.Add({ DecalGPU
			? std::format("  Decal GPU {:.3f} ms (avg, max {:.3f})", DecalGPU->GetRecentAverageMs(), DecalGPU->GetMaxMs())
			: FString("  Decal GPU -"), ValueColor });
	}

	if (FStatOverlay::IsEnabled(EStatFlags::Skinning)) {
		uint32 NumCPU = 0, NumGPU = 0;
		uint64 CPUVerts = 0, GPUVerts = 0, GPUBones = 0;
		for (TObjectIterator<USkinnedMeshComponent> It; It; ++It) {
			if (!It->IsRegistered()) continue;
			const FSkeletalMeshObject* Obj = It->GetMeshObject();
			const USkeletalMesh* Mesh = It->GetSkeletalMesh();
			if (!Obj || !Mesh) continue;
			const uint64 Verts = Mesh->GetRenderData().LODRenderData[0].BindVertices.Num();
			if (Obj->GetBoneMatrixBuffer()) { ++NumGPU; GPUVerts += Verts; GPUBones += Mesh->GetNumBones(); }
			else { ++NumCPU; CPUVerts += Verts; }
		}
		const double UploadKB = (CPUVerts * sizeof(FVertex) + GPUBones * sizeof(FMatrix)) / 1024.0;

		Lines.Add({ std::format("Skinning [{}]{}", USkinnedMeshComponent::bUseGPUSkinning ? "GPU" : "CPU",
								USkinnedMeshComponent::bForceSkinUpdate ? "  (force update)" : ""), TitleColor });
		Lines.Add({ std::format("  Components  CPU {} / GPU {}", NumCPU, NumGPU), ValueColor });
		Lines.Add({ std::format("  Vertices {}   Bones(GPU) {}", CPUVerts + GPUVerts, GPUBones), ValueColor });
		Lines.Add({ std::format("  Upload {:.1f} KB/frame", UploadKB), ValueColor });

		// CPU와 GPU를 같은 형식(Last/Avg/Max)으로 그린다. 찾는 함수만 다르다.
		auto AddRows = [&](const char* Title, std::initializer_list<TStatId> Ids, auto Find) {
			ProfileLines.Add({ Title, TitleColor, { "Last", "Avg", "Max" } });
			for (TStatId Id : Ids) {
				FProfileStatLine Line{ "  " + FString(Id.GetName()), ValueColor };
				if (const FCycleStatData* D = Find(Id)) {
					Line.Values[0] = std::format("{:.2f}", D->GetLastMs());
					Line.Values[1] = std::format("{:.2f}", D->GetRecentAverageMs());
					Line.Values[2] = std::format("{:.2f}", D->GetMaxMs());
				}
				else {
					Line.Values[0] = Line.Values[1] = Line.Values[2] = "-";
				}
				ProfileLines.Add(Line);
			}
			};
		AddRows("Skinning CPU (ms)",
			{ SkinningStats::STAT_BoneTransforms, SkinningStats::STAT_CpuSkinning,
			  SkinningStats::STAT_SkinUpload,     SkinningStats::STAT_BoneUpload,
			  SkinningStats::STAT_CpuSkinningPicking },
			[](TStatId Id) { return FStatRegistry::Find(Id); });
		AddRows("Skinning GPU (ms, ~4 frames behind)",
			{ GPUStats::STAT_GPU_ShadowDepths, GPUStats::STAT_GPU_DepthPrepass, GPUStats::STAT_GPU_ForwardOpaque },
			[](TStatId Id) { return FStatRegistry::FindGPU(Id); });
		Lines.Add({ std::format("  GPU dropped frames {}", FRealtimeGPUProfiler::GetDroppedFrames()), ValueColor });
	}

	if (Lines.Num() == 0 && ProfileLines.Num() == 0)
		return;

	// 제어 위젯 한 줄 아래에서 시작해 Combo와 겹치지 않게 한다.
	const float LineHeight = ImGui::GetTextLineHeight();
	const ImVec2 Origin{ViewMin.x + StatOverlayMargin,
			ViewMin.y + StatOverlayMargin + ImGui::GetFrameHeight() +
			StatOverlayMargin};

	// 최대 너비 계산
	float MaxWidth = 0.0f;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
		MaxWidth =
				std::max(MaxWidth, ImGui::CalcTextSize(Lines[Index].Text.c_str()).x);

	const float MinValueWidth = ImGui::CalcTextSize("00000.00").x;
	float NameWidth = 0.0f;
	float ValueWidths[3] = {MinValueWidth, MinValueWidth, MinValueWidth};
	bool bHasColumns = false;

	for (const FProfileStatLine &Line : ProfileLines) {
		const float TextWidth = ImGui::CalcTextSize(Line.Text.c_str()).x;
		bHasColumns = true;
		NameWidth = std::max(NameWidth, TextWidth);
		for (int32 Column = 0; Column < 3; ++Column)
			ValueWidths[Column] = std::max(
					MinValueWidth, ImGui::CalcTextSize(Line.Values[Column].c_str()).x);
	}

	float ColumnRight[3]{};
	if (bHasColumns) {
		float Width = NameWidth;
		for (int32 Column = 0; Column < 3; ++Column) {
			Width += ImGui::GetFontSize() + ValueWidths[Column];
			ColumnRight[Column] = Origin.x + Width;
		}
		MaxWidth = std::max(MaxWidth, Width);
	}

	const ImVec2 BackgroundMin{Origin.x - StatOverlayPadding,
			Origin.y - StatOverlayPadding};
	const ImVec2 BackgroundMax{
			Origin.x + MaxWidth + StatOverlayPadding,
			Origin.y +
					LineHeight * static_cast<float>(Lines.Num() + ProfileLines.Num()) +
					StatOverlayPadding};
	DrawList->AddRectFilled(BackgroundMin, BackgroundMax,
			StatOverlayBackgroundColor, 4.0f);

	// Lines 그리기
	for (int32 Index = 0; Index < Lines.Num(); ++Index) {
		DrawList->AddText(
				{Origin.x, Origin.y + LineHeight * static_cast<float>(Index)},
				Lines[Index].Color, Lines[Index].Text.c_str());
	}

	// ProfileLines 그리기
	for (int32 Index = 0; Index < ProfileLines.Num(); ++Index) {
		const FProfileStatLine &Line = ProfileLines[Index];
		const float Y =
				Origin.y + LineHeight * static_cast<float>(Lines.Num() + Index);
		DrawList->AddText({Origin.x, Y}, Line.Color, Line.Text.c_str());

		for (int32 Column = 0; Column < 3; ++Column) {
			const FString &Text = Line.Values[Column];
			const float TextWidth = ImGui::CalcTextSize(Text.c_str()).x;
			DrawList->AddText({ColumnRight[Column] - TextWidth, Y}, Line.Color,
			Text.c_str());
		}
	}
}

// UE5 뷰포트 위쪽 툴바. 왼쪽은 기즈모 도구, 오른쪽은 이 뷰의 카메라·보기 설정이다.
// [이동 회전 크기][좌표계]  ...........  [카메라 Perspective ▾][속도 ▾][Lit ▾][눈 ▾][레이아웃]
void FViewportsPanel::DrawViewportToolbar(int32 ViewIndex, FEditorViewportClient* Client, const ImVec2& ViewMin, const ImVec2& ViewMax)
{
	const float S = EditorIcons::GetScale();
	const float BarHeight = GetToolbarHeight();
	const float ButtonHeight = BarHeight - 6.0f * S;
	const float Pad = 6.0f * S;
	const ImVec2 BarMax(ViewMax.x, ViewMin.y + BarHeight);

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(ViewMin, BarMax, ImGui::GetColorU32(ImGuiCol_TitleBg, 0.94f));
	// 4분할일 때 활성 뷰는 툴바 아래쪽에 강조선을 긋는다.
	if (ViewIndex == ActiveViewIndex && CurrentLayoutMode == ELayoutMode::QuadSplit)
		DrawList->AddLine(ImVec2(ViewMin.x, BarMax.y - 1.0f), ImVec2(BarMax.x, BarMax.y - 1.0f), ImGui::GetColorU32(ImGuiCol_SliderGrab), 2.0f * S);
	if (ImGui::IsMouseHoveringRect(ViewMin, BarMax))
		bToolbarHovered = true;

	const ImU32 IconColor = ImGui::GetColorU32(ImGuiCol_Text, 0.8f);
	const ImU32 ActiveColor = ImGui::GetColorU32(ImGuiCol_SliderGrab);
	FEditorModeTools& ModeTools = GLevelEditorModeTools();

	// 알약 모양 묶음 배경 (UE5 툴바 그룹)
	auto BeginPill = [DrawList]() { DrawList->ChannelsSplit(2); DrawList->ChannelsSetCurrent(1); ImGui::BeginGroup(); };
	auto EndPill = [DrawList, S]()
	{
		ImGui::EndGroup();
		DrawList->ChannelsSetCurrent(0);
		DrawList->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(ImGuiCol_FrameBg), 4.0f * S);
		DrawList->ChannelsMerge();
	};
	// 메뉴 팝업은 방금 그린 버튼 바로 아래에 연다. 오른쪽 메뉴는 버튼 오른쪽 끝에 맞춘다.
	auto OpenBelow = [S](const char* PopupId, bool bAlignRight)
	{
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		ImGui::SetNextWindowPos(ImVec2(bAlignRight ? Max.x : Min.x, Max.y + 2.0f * S), ImGuiCond_Appearing,
			ImVec2(bAlignRight ? 1.0f : 0.0f, 0.0f));
		ImGui::OpenPopup(PopupId);
	};

	ImGui::PushID(1000 + ViewIndex);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f * S, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f * S, 6.0f * S));

	// ---------------- 왼쪽: 기즈모 도구 ----------------
	const float ButtonY = ViewMin.y + (BarHeight - ButtonHeight) * 0.5f;
	ImGui::SetCursorScreenPos(ImVec2(ViewMin.x + Pad, ButtonY));

	// Transform Tools 메뉴 (UE5 뷰포트 툴바 맨 왼쪽 ▾): 도구와 좌표계를 고르고 단축키를 보여 준다.
	const UE::Widget::EWidgetMode WidgetMode = ModeTools.GetWidgetMode();
	const bool bWorld = ModeTools.GetCoordSystem() == COORD_World;
	struct FToolEntry { const char* Id; const char* Label; EEditorIcon Icon; UE::Widget::EWidgetMode Mode; const char* Key; };
	const FToolEntry Tools[] = {
		{ "##Select",    "Select Mode",    EEditorIcon::Select,    UE::Widget::WM_None,      "Q" },
		{ "##Translate", "Translate Mode", EEditorIcon::Translate, UE::Widget::WM_Translate, "W" },
		{ "##Rotate",    "Rotate Mode",    EEditorIcon::Rotate,    UE::Widget::WM_Rotate,    "E" },
		{ "##Scale",     "Scale Mode",     EEditorIcon::Scale,     UE::Widget::WM_Scale,     "R" },
	};

	BeginPill();
	if (EditorIcons::Button("##TransformTools", EEditorIcon::SelectMode, IconColor, nullptr, true,
		ImGui::IsPopupOpen("##TransformToolsMenu"), ButtonHeight))
		OpenBelow("##TransformToolsMenu", false);
	EndPill();
	ImGui::SetItemTooltip("Transform Tools");
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * S, 1.0f * S));
	if (ImGui::BeginPopup("##TransformToolsMenu"))
	{
		EditorIcons::MenuSection("Transform Tools");
		for (const FToolEntry& Tool : Tools)
		{
			if (EditorIcons::MenuRow(Tool.Label, Tool.Icon, WidgetMode == Tool.Mode, true, false, Tool.Key, true))
				ModeTools.SetWidgetMode(Tool.Mode);
		}
		ImGui::Spacing();
		EditorIcons::MenuSection("Coordinate Space");
		if (EditorIcons::MenuRow("World", EEditorIcon::World, bWorld, true, false, nullptr, true))
			ModeTools.SetCoordSystem(COORD_World);
		if (EditorIcons::MenuRow("Local", EEditorIcon::Local, !bWorld, true, false, nullptr, true))
			ModeTools.SetCoordSystem(COORD_Local);
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();

	// 도구 버튼 4개 (지금 도구는 강조색)
	ImGui::SameLine(0.0f, 4.0f * S);
	for (int32 I = 0; I < IM_ARRAYSIZE(Tools); ++I)
	{
		if (I > 0) ImGui::SameLine();
		const bool bActive = WidgetMode == Tools[I].Mode;
		if (EditorIcons::Button(Tools[I].Id, Tools[I].Icon, bActive ? ActiveColor : IconColor, nullptr, false, false, ButtonHeight))
			ModeTools.SetWidgetMode(Tools[I].Mode);
		ImGui::SetItemTooltip("%s (%s)", Tools[I].Label, Tools[I].Key);
	}

	// 좌표계: 누르면 World/Local을 바꾼다.
	ImGui::SameLine(0.0f, 6.0f * S);
	BeginPill();
	if (EditorIcons::Button("##CoordSpace", bWorld ? EEditorIcon::World : EEditorIcon::Local, IconColor, nullptr, false, false, ButtonHeight))
		ModeTools.SetCoordSystem(bWorld ? COORD_Local : COORD_World);
	ImGui::SetItemTooltip(bWorld ? "World space (click for Local)" : "Local space (click for World)");
	EndPill();

	// 통계 리셋 (통계 오버레이가 켜진 활성 뷰에서만)
	if (ViewIndex == ActiveViewIndex && !Client->IsLightCulling() &&
		FStatOverlay::IsAnyEnabled() &&
		(FStatOverlay::IsEnabled(EStatFlags::Profile) || FStatRegistry::Find(EditorStats::STAT_PickingTime)))
	{
		ImGui::SameLine(0.0f, 6.0f * S);
		if (EditorIcons::Button("##ResetStats", EEditorIcon::Reset, IconColor, "Stats", false, false, ButtonHeight))
			FStatRegistry::Reset();
		ImGui::SetItemTooltip("Reset Stats");
		StatResetButtonMin = ImGui::GetItemRectMin();
		StatResetButtonMax = ImGui::GetItemRectMax();
		bStatResetButtonVisible = true;
	}
	const float LeftEndX = ImGui::GetItemRectMax().x;

	// ---------------- 오른쪽: 이 뷰의 보기 설정 ----------------
	static constexpr const char* ViewModeLabels[] = { "Lit", "Wireframe", "Scene Depth", "World Normal", "Light Culling", "Cascade", "Shadow Mask" };
	const int32 TypeIndex = static_cast<int32>(Client->GetViewportType());
	const int32 ModeIndex = static_cast<int32>(Client->GetViewMode());
	const char* TypeLabel = ViewportTypeLabels[TypeIndex];
	const char* ModeLabel = (ModeIndex >= 0 && ModeIndex < IM_ARRAYSIZE(ViewModeLabels)) ? ViewModeLabels[ModeIndex] : "View";
	char SpeedLabel[16];
	std::snprintf(SpeedLabel, sizeof(SpeedLabel), "%.2f", CameraSpeed ? *CameraSpeed : 1.0f);

	// 뷰가 좁아 글자까지 넣으면 왼쪽과 겹칠 때는 아이콘만 그린다.
	auto RightWidth = [&](bool bWithLabels)
	{
		const float Gap = 4.0f * S;
		return EditorIcons::CalcButtonWidth(true, bWithLabels ? TypeLabel : nullptr, true, ButtonHeight) + Gap
			+ EditorIcons::CalcButtonWidth(true, (bWithLabels && CameraSpeed) ? SpeedLabel : nullptr, true, ButtonHeight) + Gap
			+ EditorIcons::CalcButtonWidth(true, bWithLabels ? ModeLabel : nullptr, true, ButtonHeight) + Gap
			+ EditorIcons::CalcButtonWidth(true, nullptr, true, ButtonHeight) + Gap
			+ EditorIcons::CalcButtonWidth(true, nullptr, false, ButtonHeight);
	};
	const bool bLabels = ViewMax.x - Pad - RightWidth(true) > LeftEndX + 12.0f * S;
	const float RightStartX = ImMax(LeftEndX + 12.0f * S, ViewMax.x - Pad - RightWidth(bLabels));

	ImGui::SetCursorScreenPos(ImVec2(RightStartX, ButtonY));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * S, 0.0f));

	// 뷰 종류
	if (EditorIcons::Button("##ViewType", EEditorIcon::Camera, IconColor, bLabels ? TypeLabel : nullptr, true,
		ImGui::IsPopupOpen("##ViewTypeMenu"), ButtonHeight))
		OpenBelow("##ViewTypeMenu", true);
	ImGui::SetItemTooltip("Viewport Type: %s", TypeLabel);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * S, 1.0f * S));
	if (ImGui::BeginPopup("##ViewTypeMenu"))
	{
		EditorIcons::MenuSection("Perspective");
		if (EditorIcons::MenuRow("Perspective", EEditorIcon::Camera, TypeIndex == 0))
			Client->SetViewportType(ELevelViewportType::Perspective);
		ImGui::Spacing();
		EditorIcons::MenuSection("Orthographic");
		for (int32 Type = 1; Type < IM_ARRAYSIZE(ViewportTypeLabels); ++Type)
		{
			if (EditorIcons::MenuRow(ViewportTypeLabels[Type], EEditorIcon::LayoutSingle, TypeIndex == Type))
				Client->SetViewportType(static_cast<ELevelViewportType>(Type));
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();

	// 카메라 속도와 시야각
	ImGui::SameLine();
	if (EditorIcons::Button("##CameraSpeed", EEditorIcon::CameraSpeed, IconColor, (bLabels && CameraSpeed) ? SpeedLabel : nullptr, true,
		ImGui::IsPopupOpen("##CameraMenu"), ButtonHeight))
		OpenBelow("##CameraMenu", true);
	ImGui::SetItemTooltip("Camera Speed and Field of View");
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * S, 4.0f * S));
	if (ImGui::BeginPopup("##CameraMenu"))
	{
		if (CameraSpeed)
		{
			EditorIcons::MenuSection("Camera Speed");
			ImGui::SetNextItemWidth(200.0f * S);
			ImGui::SliderFloat("##Speed", CameraSpeed, 0.1f, 10.0f, "%.2f");
		}
		if (Client->IsPerspective())
		{
			EditorIcons::MenuSection("Field of View");
			float Fov = Client->GetViewFOV();
			ImGui::SetNextItemWidth(200.0f * S);
			if (ImGui::SliderFloat("##Fov", &Fov, 30.0f, 120.0f, "%.0f"))
				Client->SetViewFOV(Fov);
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();

	// 뷰 모드
	ImGui::SameLine();
	if (EditorIcons::Button("##ViewMode", EEditorIcon::Lit, IconColor, bLabels ? ModeLabel : nullptr, true,
		ImGui::IsPopupOpen("##ViewModeMenu"), ButtonHeight))
		OpenBelow("##ViewModeMenu", true);
	ImGui::SetItemTooltip("View Mode: %s", ModeLabel);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * S, 1.0f * S));
	if (ImGui::BeginPopup("##ViewModeMenu"))
	{
		EditorIcons::MenuSection("View Mode");
		for (int32 Mode = 0; Mode < IM_ARRAYSIZE(ViewModeLabels); ++Mode)
		{
			if (Mode == 2)
			{
				ImGui::Spacing();
				EditorIcons::MenuSection("Buffer Visualization");
			}
			if (EditorIcons::MenuRow(ViewModeLabels[Mode], Mode == 0 ? EEditorIcon::Lit : EEditorIcon::None, ModeIndex == Mode))
				Client->SetViewMode(static_cast<EViewModeIndex>(Mode));
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();

	// 쇼 플래그
	ImGui::SameLine();
	if (EditorIcons::Button("##ShowFlags", EEditorIcon::Eye, IconColor, nullptr, true,
		ImGui::IsPopupOpen("##ShowMenu"), ButtonHeight))
		OpenBelow("##ShowMenu", true);
	ImGui::SetItemTooltip("Show");
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * S, 1.0f * S));
	if (ImGui::BeginPopup("##ShowMenu"))
	{
		FEngineShowFlags& Flags = Client->GetEngineShowFlags();
		struct FFlagRow { const char* Label; bool* Value; };
		const FFlagRow Rows[] = {
			{ "Rendering", &Flags.Rendering },
			{ "Skeletal Meshes", &Flags.SkeletalMeshes },
			{ "Decals", &Flags.Decals },
			{ "Fog", &Flags.Fog },
			{ "Anti-aliasing", &Flags.AntiAliasing },
			{ "Vignette", &Flags.Vignette },
			{ "Camera Overlay", &Flags.CameraOverlay },
		};
		EditorIcons::MenuSection("Show Flags");
		for (const FFlagRow& Row : Rows)
		{
			if (EditorIcons::MenuRow(Row.Label, EEditorIcon::None, *Row.Value, true, true))
				*Row.Value = !*Row.Value;
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar();

	// 레이아웃: 4분할이면 이 뷰만 크게, 한 칸이면 다시 4분할
	ImGui::SameLine();
	const bool bQuad = CurrentLayoutMode == ELayoutMode::QuadSplit;
	if (EditorIcons::Button("##Layout", bQuad ? EEditorIcon::LayoutSingle : EEditorIcon::LayoutQuad, IconColor, nullptr, false, false, ButtonHeight))
	{
		RequestedLayoutMode = bQuad ? ELayoutMode::Single : ELayoutMode::QuadSplit;
		RequestedSingleViewIndex = ViewIndex;
		bHasLayoutRequest = true;
	}
	ImGui::SetItemTooltip(bQuad ? "Maximize this viewport" : "Restore four viewports");

	ImGui::PopStyleVar(3);
	ImGui::PopID();
}
