#include "EnginePCH.h"
#include "Particles/ParticleCurve.h"

#include <algorithm>
#include <cmath>

namespace
{
	// UCurveFloat::EvalSegment와 같은 식
	float EvalSegment(const FParticleCurveKey& L, const FParticleCurveKey& R, float Time)
	{
		const float Length = R.Time - L.Time;
		if (Length <= 1e-6f) return L.Value;
		const float T = std::clamp((Time - L.Time) / Length, 0.0f, 1.0f);

		if (L.InterpMode == EParticleCurveInterp::Constant) return L.Value;
		if (L.InterpMode == EParticleCurveInterp::Linear) return L.Value + (R.Value - L.Value) * T;

		const float T2 = T * T, T3 = T2 * T;
		return (2 * T3 - 3 * T2 + 1) * L.Value + (T3 - 2 * T2 + T) * L.LeaveTangent * Length
			+ (-2 * T3 + 3 * T2) * R.Value + (T3 - T2) * R.ArriveTangent * Length;
	}
}

float FParticleCurve::Eval(float Time, float Default) const
{
	const int32 Num = static_cast<int32>(Keys.Num());
	if (Num == 0) return Default;
	if (Time <= Keys[0].Time) return Keys[0].Value;
	if (Time >= Keys[Num - 1].Time) return Keys[Num - 1].Value;

	int32 Right = 1;                                   // 키는 보통 10개 이하라 선형 탐색으로 충분
	while (Right < Num && Keys[Right].Time < Time) ++Right;
	return EvalSegment(Keys[Right - 1], Keys[Right], Time);
}

int32 FParticleCurve::AddKey(float Time, float Value)
{
	FParticleCurveKey Key;
	Key.Time = Time;
	Key.Value = Value;

	int32 Index = 0;
	while (Index < static_cast<int32>(Keys.Num()) && Keys[Index].Time < Time) ++Index;
	Keys.Insert(Key, Index);
	AutoSetTangents();
	return Index;
}

void FParticleCurve::RemoveKeyAt(int32 Index)
{
	if (Index < 0 || Index >= static_cast<int32>(Keys.Num())) return;
	Keys.RemoveAt(Index);
	AutoSetTangents();
}

void FParticleCurve::AutoSetTangents()
{
	const int32 Num = static_cast<int32>(Keys.Num());
	for (int32 i = 0; i < Num; ++i)
	{
		FParticleCurveKey& Key = Keys[i];
		if (!Key.bAutoTangent) continue;

		float Slope = 0.0f;                               // 양 끝은 평평하게 (Cascade Auto와 같은 모양)
		if (i > 0 && i < Num - 1)
		{
			const float Dt = Keys[i + 1].Time - Keys[i - 1].Time;
			Slope = Dt > 1e-6f ? (Keys[i + 1].Value - Keys[i - 1].Value) / Dt : 0.0f;
		}
		Key.ArriveTangent = Key.LeaveTangent = Slope;
	}
}

void FParticleCurve::GetValueRange(float& OutMin, float& OutMax) const
{
	OutMin = OutMax = Keys.IsEmpty() ? 0.0f : Keys[0].Value;
	if (Keys.Num() < 2) return;

	const float T0 = Keys[0].Time;
	const float T1 = Keys[Keys.Num() - 1].Time;
	for (int32 i = 0; i <= 64; ++i)
	{
		const float V = Eval(T0 + (T1 - T0) * i / 64.0f, 0.0f);
		OutMin = std::min(OutMin, V);
		OutMax = std::max(OutMax, V);
	}
}