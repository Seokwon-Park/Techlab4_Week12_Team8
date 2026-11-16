#pragma once

#include "Core/Types.h"

// 정수 두 개로 된 화면 좌표나 크기. UE의 FIntPoint와 같다.
struct FIntPoint
{
	int32 X = 0;
	int32 Y = 0;

	FIntPoint() = default;
	FIntPoint(int32 InX, int32 InY) : X(InX), Y(InY) {}

	bool operator==(const FIntPoint& Other) const { return X == Other.X && Y == Other.Y; }
	bool operator!=(const FIntPoint& Other) const { return !(*this == Other); }
};
