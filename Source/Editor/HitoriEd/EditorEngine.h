#pragma once

#include "Engine/Engine.h"
#include "Core/Window.h"
#include "Core/Types.h"

#include "Render/Renderer.h"
#include "Render/SceneRenderer.h"
#include "Render/RenderDevice.h"
#include "Render/Swapchain.h"
#include "Editor/EditorUI/ImGuiRenderer.h"
#include "Editor/Rendering/GridRenderer.h"
#include "Editor/Gizmo/GizmoRenderer.h"
#include "Render/LineBatcher.h"
#include "Editor/Rendering/SceneDepthRenderer.h"
#include "Editor/Rendering/CascadeFloorRenderer.h"
#include "Render/SceneRenderOptions.h"
#include "Render/SceneViewTargets.h"
#include "Render/RenderGraph/RenderGraph.h"


#include "Editor/LevelEditor/LevelEditorTab.h"
#include "Editor/AssetEditor/EditorManager.h"
#include "Editor/OutputLog/OutputLogPanel.h"
#include "Editor/Details/DetailsPanel.h"
#include "Editor/EditorControls/EditorControlsPanel.h"
#include "Editor/Settings/SettingsPanel.h"
#include "Editor/Sequencer/SequencerPanel.h"
#include "Editor/Viewports/ViewportsPanel.h"
#include "Editor/LevelViewportLayout.h"
#include "Editor/ContentDrawer/ContentDrawerPanel.h"
#include "Editor/UIEditor/UIEditor.h"

#include "Editor/Rendering/Outline.h"
#include "Editor/Rendering/OutLineRenderer.h"
#include "Editor/Outliner/OutlinerPanel.h"

#include "PlayInEditorDataTypes.h"
#include "Core/Misc/Optional.h"

#include "Text/TextRenderer.h"

#include "Editor/HitoriEd/GamePackager.h"

#include <Engine\GameViewportClient.h>

#include "UI/UIRenderer.h"
#include "UI/UIDrawList.h"

#include "Editor/Selection.h"
#include "EditorEngine.generated.h"

inline constexpr int32 InvalidViewIndex = -1;

UCLASS()
class UEditorEngine : public UEngine
{
	GENERATED_BODY()
public:
	FEngineConfig GetConfig() const override;
	bool Init() override;
	void Tick(float DeltaTime) override;
	void PreExit() override;

	// Active View의 입력과 Picking 결과만 Gizmo 및 선택 상태에 반영한다.
	void UpdateGizmoAndPicking();
	// PIE 게임 화면 위에 Settings > Debug Draw (PIE)에서 켠 디버그 표시를 그린다.
	void DrawPIEDebug(UWorld* World, const FMatrix& ViewProjection);
	void DeleteActor(AActor* Actor);
	void DeleteComponent(UActorComponent* Component);

	//Play 버튼을 눌렀을때 Play Session 실행을 요청한다.
	inline void RequestPlaySession() { bPlaySessionRequested = true; }
	inline void RequestEndPlayMap() { bRequestEndPlayMapQueued = true; bIsPaused = false; }

	// 대기 중인 Play 요청이 있으면 PIE를 시작한다. Tick이 프레임 시작에 부른다.
	void StartQueuedPlaySessionRequest();
	//실제 PIE 를 실행
	void StartPlayInEditorSession();
	UWorld* CreatePIEWorldByDuplication(FWorldContext& PIEContext, UWorld* InEditorWorld);

	void OnActiveWorldChanged();

	void EndPlayMap();

	// PIE(게임을 조작)와 SIE(에디터처럼 둘러보고 선택)를 오간다. F8이 부른다.
	// UE의 UEditorEngine::ToggleBetweenPIEandSIE와 같은 자리다.
	void ToggleBetweenPIEandSIE();

	// 아래 셋은 에디터 전체가 공유하는 상태를 읽는 접근자다. 레벨 뷰포트 클라이언트가 그릴 때 쓴다.
	// UE도 선택과 설정은 에디터 전역이 갖고 뷰포트 클라이언트가 읽어 간다(GEditor->GetSelectedActors(),
	// GetDefault<ULevelEditorViewportSettings>()). 다만 이 세 함수의 이름은 UE에 없는, 이 엔진 고유의 이름이다.
	// 설정 패널이 아직 없으면 nullptr이다.
	const FEditorSettings* GetEditorSettings() const { return SettingsPanel ? &SettingsPanel->GetSettings() : nullptr; }
	// 지금 선택된 컴포넌트와 조작 모드를 가진 기즈모
	FGizmo* GetGizmo() const { return Gizmo.get(); }
	// 지금 선택된 메시의 외곽선
	FOutline* GetOutline() const { return Outline.get(); }

	// 선택된 액터와 컴포넌트의 집합. 패널과 뷰포트는 선택을 따로 기억하지 않고 여기서 읽는다.
	// UE의 UEditorEngine::GetSelectedActors, GetSelectedComponents와 같다.
	USelection* GetSelectedActors() const { return SelectedActors; }
	USelection* GetSelectedComponents() const { return SelectedComponents; }

	// 액터 하나를 선택하거나 선택에서 뺀다. 액터 선택이 바뀌면 컴포넌트 선택은 비운다.
	// bNotify가 true면 NoteSelectionChange를 불러 기즈모, 외곽선, 디테일 패널을 새 선택에 맞춘다.
	// 이 엔진은 한 번에 하나만 선택하므로, 다른 액터를 고를 때는 SelectNone(false)을 먼저 부른다. UE에서도 클릭 선택은 그렇게 한다.
	// UE의 UEditorEngine::SelectActor와 같다. UE에는 뒤에 인자가 더 있다(bSelectEvenIfHidden, bForceRefresh).
	void SelectActor(AActor* Actor, bool bInSelected, bool bNotify);
	// 컴포넌트 하나를 선택하거나 선택에서 뺀다. UE의 UEditorEngine::SelectComponent와 같다.
	void SelectComponent(UActorComponent* Component, bool bInSelected, bool bNotify);
	// 액터와 컴포넌트 선택을 모두 비운다. UE의 UEditorEngine::SelectNone와 같다. UE에는 뒤에 인자가 더 있다.
	void SelectNone(bool bNoteSelectionChange);
	// 선택이 바뀐 뒤에 부른다. 기즈모 대상, 외곽선 대상, 디테일 패널의 대상을 지금의 선택에 맞춘다.
	// UE의 UEditorEngine::NoteSelectionChange와 같은 자리다. UE는 여기서 이벤트를 알리고 각 패널이 받아서 스스로 갱신한다.
	void NoteSelectionChange();

	// 에디터 월드가 들어 있는 월드 컨텍스트. UE에서도 public이라 GEditor->GetEditorWorldContext().World()로 에디터 월드를 얻는다.
	FWorldContext& GetEditorWorldContext();

	bool IsPlaying() const { return PlayWorld != nullptr; }
	EPIEState GetPIEState() const
	{
		if (!PlayWorld) return EPIEState::Stopped;
		if (bIsPaused) return EPIEState::Paused;
		return IsSimulatingInEditor() ? EPIEState::Simulating : EPIEState::Playing;
	}

protected:
	void PreLoadMap(FWorldContext& Context) override;
	void PostLoadMap(FWorldContext& Context) override;

private:
	// 에디터 월드와 그 월드 컨텍스트를 만든다. UE의 UEditorEngine::InitEditor에 해당한다.
	void InitEditor();
	// 아래 다섯은 Init이 순서대로 부르는 UI 초기화 단계다. UE에서는 레벨 에디터 모듈과 Slate가 맡는 일이라
	// 대응하는 UE 함수가 없는, 이 엔진 고유의 이름이다.
	void InitLevelEditorTab();
	void InitImGui();
	void InitEditorRenderers();
	void InitPanels();
	void InitCommands();

	// View 하나를 그린다. UE의 UEditorEngine::UpdateSingleViewportClient에 해당한다.
	void UpdateSingleViewportClient(FEditorViewportClient* ViewClient, int32 ViewIndex);
	// PIE View를 게임 카메라 상태와 동기화한다.
	void UpdatePIEViewCamera();
	void TickPIEInput(float DeltaTime);

	// UE PIE 마우스 캡처: Viewport를 클릭(또는 PIE 시작)하면 커서를 숨기고 PIE 영역에 가둔 채
	// 매 프레임 중앙으로 되돌려 상대 이동량을 얻는다. Shift+F1 / 포커스 상실 / PIE 종료로 푼다.
	void BeginPIEMouseCapture();
	void EndPIEMouseCapture();
	// 캡처 영역(PIE View 또는 PIE 창 클라이언트)을 화면 좌표로 구한다
	bool GetPIECaptureScreenRect(int32& OutLeft, int32& OutTop, int32& OutRight, int32& OutBottom) const;
	// 커서를 영역에 가두고 중앙으로 옮긴다. 영역이 없으면 false
	bool RecenterPIEMouseCapture();

	// 별도 PIE 창(New Editor Window)을 만들고 정리한다.
	bool CreatePIEWindow();
	void DestroyPIEWindow();
	// 창 크기에 맞춰 Swapchain과 오프스크린 타깃을 다시 만든다. 그릴 수 없으면 false다.
	bool UpdatePIEWindowTargets();
	// PIE 창의 오프스크린 결과를 백버퍼로 복사한다. Present는 Tick의 끝에서 한다.
	void CopyPIEWindowToBackbuffer();
	bool IsPIEInWindow() const { return PIEWindow != nullptr; }



	FWorldContext* GetPIEWorldContext(int32 WorldPIEInstance = 0);
	UWorld* GetActiveWorld() const { return PlayWorld ? PlayWorld : EditorWorld; }

	// PIE 중이고 일시정지가 아닌 View는 Gizmo·Picking 등 에디터 상호작용을 하지 않는다.
	bool IsPlayingView(int32 ViewIndex) const { return PlayWorld && ViewIndex == PIEViewIndex && !bIsPaused && !IsSimulatingInEditor(); }
	bool IsSimulatingInEditor() const { return bIsSimulatingInEditor; }

	// FEngineLoop 소유. OnInit에서 받아 둔다.
	FWindow* MainWindow = nullptr;
	FSwapchain* MainWindowSC = nullptr;
	FRenderer* Renderer = nullptr;

	TUniquePtr<FEditorManager> EditorManager;

	TUniquePtr<FImGuiRenderer> ImGuiRenderer;
	TUniquePtr<FOutline> Outline;
	TUniquePtr<FGizmo> Gizmo;

	// 선택된 액터와 컴포넌트. InitEditorRenderers에서 만들고 PreExit에서 지운다.
	USelection* SelectedActors = nullptr;
	USelection* SelectedComponents = nullptr;


	FLevelEditorTab* LevelEditorTab = nullptr;
	FOutputLogPanel* OutputLogPanel = nullptr;

	FDetailsPanel* DetailsPanel = nullptr;
	FEditorControlsPanel* EditorControlsPanel = nullptr;
	FSettingsPanel* SettingsPanel = nullptr;
	FSequencerPanel* SequencerPanel = nullptr;
	FViewportsPanel* ViewportsPanel = nullptr;
	FSplitRatio ViewportSplitRatio{0.5f, 0.5f};
	ELayoutMode ViewportLayoutMode = ELayoutMode::QuadSplit;
	int32 SingleViewportIndex = 0;
	TArray<FEditorViewportClient*> AllViewportClients;
	FOutlinerPanel* OutlinerPanel = nullptr;
	FContentDrawerPanel* ContentDrawerPanel = nullptr;

	UWorld* EditorWorld = nullptr;
	UWorld* PlayWorld = nullptr;
	// PIE 시작 시점의 편집 View. PIE가 아니면 InvalidViewIndex다.
	int32 PIEViewIndex = -1;
	int32 PIEStartEditorViewIndex = -1;

	// Play 버튼이 다음 세션을 띄울 위치
	EPlayModeType PlayMode = EPlayModeType::InViewport;

	// PIE 게임 화면의 뷰포트. PIE를 시작할 때 만들고 끝낼 때 버린다. 에디터 View 안에서 플레이할 때와 별도 창일 때 모두 쓴다.
	// 그릴 대상(텍스처)과 크기를 갖고, 그리는 일은 PIE 월드 컨텍스트의 게임 뷰포트 클라이언트에게 맡긴다.
	// 이 엔진 고유의 이름이다. UE는 PIE 화면의 FSceneViewport를 Slate 위젯이 갖는다
	// (별도 창은 FSlatePlayInEditorInfo::SlatePlayInEditorWindowViewport, 에디터 안은 SLevelViewport::ActiveViewport).
	TUniquePtr<FViewport> PlayInEditorViewport;

	// New Editor Window(PIE) 모드에서만 존재한다. Swapchain이 창 핸들을 쓰므로 창보다 먼저 해제한다.
	TUniquePtr<FWindow> PIEWindow;
	TUniquePtr<FSwapchain> PIESwapchain;
	uint32 PIETargetWidth = 0;
	uint32 PIETargetHeight = 0;
	bool bPIEWindowRendered = false;

	FGamePackager Packager;

	FEditorCommand Save;
	FEditorCommand SaveAs;
	FEditorCommand NewScene;
	FEditorCommand OpenSceneCommand;  // OpenScene()과 이름이 겹치지 않게 Command를 붙인다

	FEditorCommand Play;
	FEditorCommand Pause;
	FEditorCommand Resume;
	FEditorCommand StepFrame;
	FEditorCommand Stop;

	bool bPlaySessionRequested = false;
	bool bRequestEndPlayMapQueued = false;
	bool bStepRequested = false;
	bool bIsPaused = false;
	// PIE 중 SIE 상태인지. UE의 UEditorEngine::bIsSimulatingInEditor와 같다.
	// ToggleBetweenPIEandSIE가 바꾸고, PIE가 끝나거나 PIE 월드가 교체되면 false로 돌아간다.
	bool bIsSimulatingInEditor = false;

	void ResetSceneSelection();

	void CreateNewScene();
	void OpenScene();
	void SaveCurrentScene();
	void SaveSceneAs();

	bool bGameMouseCaptured = false;
	// PIE 시작 / SIE → 게임 복귀 시 다음 입력 틱에서 자동 캡처 (UE bCaptureMouseOnLaunch)
	bool bPendingPIEMouseCapture = false;
	// 캡처 중 커서를 되돌려 놓는 화면 좌표
	int32 PIECaptureCenterX = 0;
	int32 PIECaptureCenterY = 0;
};
