#include "EnginePCH.h"
#include "Editor/Outliner/OutlinerPanel.h"
#include "Editor/HitoriEd/Editor.h"
#include "Editor/EditorUI/EditorIcons.h"

#include "Engine/Level.h"
#include "Engine/World.h"

#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"

#include <cctype>

namespace
{
    // UE5 Outliner의 Type 열처럼 클래스 이름에서 접두사 A를 뗀다 ("AStaticMeshActor" -> "StaticMeshActor").
    const char* GetActorTypeName(const AActor* Actor)
    {
        const UClass* Class = Actor ? Actor->GetClass() : nullptr;
        if (!Class)
            return "";
        const char* Name = Class->Name.c_str();
        if (Name[0] == 'A' && std::isupper(static_cast<unsigned char>(Name[1])))
            return Name + 1;
        return Name;
    }

    bool IsActorVisible(const AActor* Actor)
    {
        const USceneComponent* Root = Actor ? Actor->GetRootComponent() : nullptr;
        return !Root || Root->IsVisible();
    }
}

bool FOutlinerPanel::Init()
{
    return true;
}

void FOutlinerPanel::Tick(float DeltaTime)
{
}

void FOutlinerPanel::OnRender()
{
    // World 참조 확인
    if (!World)
        return;

    // 현재 Level 가져오기
    ULevel* Level = World->GetCurrentLevel();
    if (!Level)
        return;

    EditorPanelUI::BeginPanel("Outliner", GetOpenFlag());

    EditorIcons::SearchBox("##OutlinerSearch", SearchBuffer, sizeof(SearchBuffer));
    DrawActors(Level);

    // UE5처럼 맨 아래에 액터 수와 선택 수를 보여 준다.
    const int32 SelectedCount = GEditor ? GEditor->GetSelectedActors()->Num() : 0;
    ImGui::TextDisabled("%u actors (%d selected)", Level->GetActorNum(), SelectedCount);

    ImGui::End();

    // Actors 순회가 완전히 끝난 뒤 삭제
    if (PendingDeleteActor)
    {
        if (DeleteCallback)
        {
            DeleteCallback(PendingDeleteActor);
        }

        PendingDeleteActor = nullptr;
    }
}

// Level의 Actor 목록을 [눈 | 이름 | 타입] 표로 그린다.
void FOutlinerPanel::DrawActors(ULevel* Level)
{
    if (!Level)
        return;

    const TArray<AActor*>& Actors = Level->GetActors();

    // 검색어에 맞는 액터만 추린다 (이름 또는 타입).
    VisibleIndices.Empty();
    for (int32 Index = 0; Index < static_cast<int32>(Actors.Num()); ++Index)
    {
        const AActor* Actor = Actors[Index];
        if (!Actor)
            continue;
        if (EditorIcons::MatchesFilter(Actor->GetName().c_str(), SearchBuffer) ||
            EditorIcons::MatchesFilter(GetActorTypeName(Actor), SearchBuffer))
        {
            VisibleIndices.Add(Index);
        }
    }

    const float S = EditorIcons::GetScale();
    const float FooterHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    const ImGuiTableFlags TableFlags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_SizingStretchProp;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f * S, 0.0f));
    if (ImGui::BeginTable("##OutlinerTable", 3, TableFlags, ImVec2(0.0f, -FooterHeight)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("##Visibility", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 18.0f * S);
        ImGui::TableSetupColumn("Item Label", ImGuiTableColumnFlags_WidthStretch, 0.62f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.38f);
        ImGui::TableHeadersRow();

        const float RowHeight = ImGui::GetTextLineHeight() + 6.0f * S;
        ImGuiListClipper Clipper;
        Clipper.Begin(VisibleIndices.Num(), RowHeight);
        while (Clipper.Step())
        {
            for (int32 Row = Clipper.DisplayStart; Row < Clipper.DisplayEnd; ++Row)
            {
                if (AActor* Actor = Actors[VisibleIndices[Row]])
                    DrawActorNode(Actor);
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// 표의 한 줄. 줄 전체가 선택 영역이고, 왼쪽 눈 아이콘은 그 위에 겹쳐 따로 눌린다.
void FOutlinerPanel::DrawActorNode(AActor* Actor)
{
    if (!Actor)
        return;

    const float S = EditorIcons::GetScale();
    const float RowHeight = ImGui::GetTextLineHeight() + 6.0f * S;
    const bool bSelected = GEditor && GEditor->GetSelectedActors()->IsSelected(Actor);
    const bool bVisible = IsActorVisible(Actor);

    ImGui::PushID(Actor);
    ImGui::TableNextRow(ImGuiTableRowFlags_None, RowHeight);

    // 줄 전체 선택 영역 (첫 칸에서 시작해 모든 열을 덮는다)
    ImGui::TableSetColumnIndex(0);
    const ImVec2 RowStart = ImGui::GetCursorPos();
    ImGui::Selectable("##Row", bSelected,
        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, RowHeight));

    // UI에서 Actor 클릭
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && GEditor)
    {
        // 누른 액터 하나만 선택한다. 뷰포트에서 클릭으로 고를 때와 같은 순서다.
        GEditor->SelectNone(false);
        GEditor->SelectActor(Actor, true, false);
        GEditor->NoteSelectionChange();
    }

    if (ImGui::BeginPopupContextItem("##ActorContext"))
    {
        if (ImGui::MenuItem("Delete"))
        {
            PendingDeleteActor = Actor; // 여기서는 바로 삭제 X
        }

        ImGui::EndPopup();
    }

    // 눈 아이콘: 루트 컴포넌트와 그 자식의 표시 여부를 바꾼다.
    ImGui::SetCursorPos(RowStart);
    const ImU32 EyeColor = ImGui::GetColorU32(ImGuiCol_Text, bVisible ? 0.75f : 0.35f);
    if (EditorIcons::Button("##Visibility", bVisible ? EEditorIcon::Eye : EEditorIcon::EyeClosed, EyeColor,
        nullptr, false, false, RowHeight))
    {
        if (USceneComponent* Root = Actor->GetRootComponent())
            Root->SetVisibility(!bVisible, true);
    }
    ImGui::SetItemTooltip(bVisible ? "Hide actor in the viewport" : "Show actor in the viewport");

    const float TextOffsetY = (RowHeight - ImGui::GetTextLineHeight()) * 0.5f;

    ImGui::TableSetColumnIndex(1);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + TextOffsetY);
    if (bVisible)
        ImGui::TextUnformatted(Actor->GetName().c_str());
    else
        ImGui::TextDisabled("%s", Actor->GetName().c_str());

    ImGui::TableSetColumnIndex(2);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + TextOffsetY);
    ImGui::TextDisabled("%s", GetActorTypeName(Actor));

    ImGui::PopID();
}
