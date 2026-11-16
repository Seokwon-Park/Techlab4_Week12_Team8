#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/Casts.h"
#include "Containers/Array.h"
#include "Selection.generated.h"

// 선택된 오브젝트의 집합. 에디터 엔진이 액터용과 컴포넌트용을 하나씩 갖는다.
// UE의 USelection과 같은 자리다. UE는 런타임 모듈의 Engine/Selection.h에 두고, 이 엔진은 에디터만 쓰므로 에디터에 둔다.
// 오브젝트를 포인터로만 기억한다. 선택된 오브젝트를 지우는 쪽이 먼저 선택에서 빼야 한다.
UCLASS()
class USelection : public UObject
{
	GENERATED_BODY()
public:
	// 선택에 넣는다. 이미 들어 있으면 아무것도 하지 않는다.
	void Select(UObject* InObject)
	{
		if (InObject && SelectedObjects.Find(InObject) == INDEX_NONE)
			SelectedObjects.Add(InObject);
	}

	// 선택에서 뺀다.
	void Deselect(UObject* InObject)
	{
		const int32 Index = SelectedObjects.Find(InObject);
		if (Index != INDEX_NONE)
			SelectedObjects.RemoveAt(static_cast<uint32>(Index));
	}

	// 선택을 모두 비운다.
	void DeselectAll() { SelectedObjects.Empty(); }

	bool IsSelected(const UObject* InObject) const
	{
		return SelectedObjects.Find(const_cast<UObject*>(InObject)) != INDEX_NONE;
	}

	int32 Num() const { return SelectedObjects.Num(); }
	UObject* GetSelectedObject(int32 InIndex) const
	{
		return (InIndex >= 0 && InIndex < SelectedObjects.Num()) ? SelectedObjects[InIndex] : nullptr;
	}

	// 선택된 것 가운데 T 타입인 첫 번째 오브젝트. 없으면 nullptr이다.
	template <typename T>
	T* GetTop() const
	{
		for (UObject* Object : SelectedObjects)
		{
			if (T* Typed = Cast<T>(Object))
				return Typed;
		}
		return nullptr;
	}

private:
	TArray<UObject*> SelectedObjects;
};
