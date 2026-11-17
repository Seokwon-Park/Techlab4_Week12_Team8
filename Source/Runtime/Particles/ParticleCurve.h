#pragma once

#include "Containers/Array.h"

// 키 → 다음 키 구간의 모양. .particle에 숫자로 저장되므로 순서를 바꾸지 말 것
enum class EParticleCurveInterp : uint8
{
	Constant,   // 0
	Linear,     // 1
	Cubic,      // 2
};

// 커브의 점 하나. InterpMode는 "이 키 → 다음 키" 구간의 모양
struct FParticleCurveKey
{
	float Time = 0.0f;
	float Value = 0.0f;
	EParticleCurveInterp InterpMode = EParticleCurveInterp::Cubic;
	bool bAutoTangent = true;        // Cascade의 Auto. false면 아래 접선을 그대로 쓴다 (Flatten = 0)
	float ArriveTangent = 0.0f;      // 값 / 시간
	float LeaveTangent = 0.0f;
};

// 키 커브 (UE: FInterpCurveFloat). Keys는 항상 Time 오름차순
struct FParticleCurve
{
	TArray<FParticleCurveKey> Keys;

	// 키가 없으면 Default. 첫 키 앞·마지막 키 뒤는 끝 값을 유지
	float Eval(float Time, float Default) const;

	// 정렬된 자리에 넣고 그 인덱스를 돌려준다
	int32 AddKey(float Time, float Value);
	void RemoveKeyAt(int32 Index);

	// bAutoTangent인 키의 접선을 이웃 키로 다시 계산 (Catmull-Rom, 양 끝은 0)
	void AutoSetTangents();

	// 그래프 범위 맞추기용. Cubic 구간의 넘침까지 보려고 샘플링한다
	void GetValueRange(float& OutMin, float& OutMax) const;
};