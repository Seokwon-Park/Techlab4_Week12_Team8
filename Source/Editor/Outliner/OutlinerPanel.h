#pragma once

#include "Editor/EditorUI/EditorPanel.h"

#include <functional>

class UWorld;
class ULevel;
class AActor;
class UActorComponent;
class UPrimitiveComponent;
class USceneComponent;

using DeleteActorCallback = std::function<void(AActor*)>;

class FOutlinerPanel : public IEditorPanel
{
public:
    FOutlinerPanel() = default;
    ~FOutlinerPanel() = default;

    void SetDeleteActorCallback(DeleteActorCallback InCallback) { DeleteCallback = InCallback;}

public:
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;
    const char* GetPanelName() const override { return "Outliner"; }

    void SetWorld(UWorld* InWorld) { World = InWorld; };

    void DrawActors(ULevel* Level);
    void DrawActorNode(AActor* Actor);

private:
    // 무엇이 선택됐는지는 패널이 기억하지 않는다. 그릴 때 GEditor->GetSelectedActors()에서 읽는다.
    UWorld* World = nullptr;

    DeleteActorCallback DeleteCallback;
    AActor* PendingDeleteActor = nullptr;

    // 검색 칸 내용과, 검색어에 맞아 이번 프레임 표에 보이는 액터의 인덱스.
    char SearchBuffer[128] = {};
    TArray<int32> VisibleIndices;
};