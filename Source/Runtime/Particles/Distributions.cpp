#include "EnginePCH.h"
#include "Particles/Distributions.h"

#include <algorithm>

namespace
{
	float ClampCurveInput(float F)
	{
		return std::clamp(F, 0.0f, 1.0f);
	}

	float MirroredMin(float InMin, float InMax, EDistributionVectorMirrorFlags Flag)
	{
		switch (Flag)
		{
		case EDistributionVectorMirrorFlags::EDVMF_Same:   return InMax;
		case EDistributionVectorMirrorFlags::EDVMF_Mirror: return -InMax;
		default:                                           return InMin;
		}
	}
}

FRawDistributionFloat FRawDistributionFloat::MakeConstant(float Value)
{
	FRawDistributionFloat Out;
	Out.Type = ERawDistributionType::Constant;
	Out.Constant = Value;
	return Out;
}

FRawDistributionFloat FRawDistributionFloat::MakeUniform(float InMin, float InMax)
{
	FRawDistributionFloat Out;
	Out.Type = ERawDistributionType::Uniform;
	Out.Min = InMin;
	Out.Max = InMax;
	return Out;
}

FRawDistributionFloat FRawDistributionFloat::MakeConstantCurve(float StartValue, float EndValue)
{
	FRawDistributionFloat Out;
	Out.Type = ERawDistributionType::ConstantCurve;
	Out.CurveStart = StartValue;
	Out.CurveEnd = EndValue;
	return Out;
}

float FRawDistributionFloat::GetValue(float F) const
{
	switch (Type)
	{
	case ERawDistributionType::Uniform:
		return FMath::FRandRange(Min, Max);

	case ERawDistributionType::ConstantCurve:
		return FMath::Lerp(CurveStart, CurveEnd, ClampCurveInput(F));

	case ERawDistributionType::Constant:
	default:
		return Constant;
	}
}

void FRawDistributionFloat::GetOutRange(float& OutMin, float& OutMax) const
{
	switch (Type)
	{
	case ERawDistributionType::Uniform:
		OutMin = std::min(Min, Max);
		OutMax = std::max(Min, Max);
		return;

	case ERawDistributionType::ConstantCurve:
		OutMin = std::min(CurveStart, CurveEnd);
		OutMax = std::max(CurveStart, CurveEnd);
		return;

	case ERawDistributionType::Constant:
	default:
		OutMin = Constant;
		OutMax = Constant;
		return;
	}
}

FRawDistributionVector FRawDistributionVector::MakeConstant(const FVector& Value)
{
	FRawDistributionVector Out;
	Out.Type = ERawDistributionType::Constant;
	Out.Constant = Value;
	return Out;
}

FRawDistributionVector FRawDistributionVector::MakeUniform(const FVector& InMin, const FVector& InMax,
	EDistributionVectorLockFlags InLockedAxes)
{
	FRawDistributionVector Out;
	Out.Type = ERawDistributionType::Uniform;
	Out.Min = InMin;
	Out.Max = InMax;
	Out.LockedAxes = InLockedAxes;
	return Out;
}

FRawDistributionVector FRawDistributionVector::MakeConstantCurve(const FVector& StartValue, const FVector& EndValue)
{
	FRawDistributionVector Out;
	Out.Type = ERawDistributionType::ConstantCurve;
	Out.CurveStart = StartValue;
	Out.CurveEnd = EndValue;
	return Out;
}

void FRawDistributionVector::GetUniformBounds(FVector& OutMin, FVector& OutMax) const
{
	OutMax = Max;
	OutMin = FVector(
		MirroredMin(Min.X, Max.X, MirrorX),
		MirroredMin(Min.Y, Max.Y, MirrorY),
		MirroredMin(Min.Z, Max.Z, MirrorZ)
	);
}

FVector FRawDistributionVector::GetValue(float F) const
{
	switch (Type)
	{
	case ERawDistributionType::Uniform:
	{
		FVector Lo;
		FVector Hi;
		GetUniformBounds(Lo, Hi);

		// 잠긴 축은 뽑지 않고 복사한다. 뽑는 순서를 정해 두려고 문장을 나눠 쓴다 (X를 먼저 뽑는다)
		switch (LockedAxes)
		{
		case EDistributionVectorLockFlags::EDVLF_XY:
		{
			const float X = FMath::FRandRange(Lo.X, Hi.X);
			const float Z = FMath::FRandRange(Lo.Z, Hi.Z);
			return FVector(X, X, Z);
		}
		case EDistributionVectorLockFlags::EDVLF_XZ:
		{
			const float X = FMath::FRandRange(Lo.X, Hi.X);
			const float Y = FMath::FRandRange(Lo.Y, Hi.Y);
			return FVector(X, Y, X);
		}
		case EDistributionVectorLockFlags::EDVLF_YZ:
		{
			const float X = FMath::FRandRange(Lo.X, Hi.X);
			const float Y = FMath::FRandRange(Lo.Y, Hi.Y);
			return FVector(X, Y, Y);
		}
		case EDistributionVectorLockFlags::EDVLF_XYZ:
		{
			const float X = FMath::FRandRange(Lo.X, Hi.X);
			return FVector(X, X, X);
		}
		case EDistributionVectorLockFlags::EDVLF_None:
		default:
			return FMath::VRandRange(Lo, Hi);
		}
	}

	case ERawDistributionType::ConstantCurve:
		return FMath::Lerp(CurveStart, CurveEnd, ClampCurveInput(F));

	case ERawDistributionType::Constant:
	default:
		return Constant;
	}
}

void FRawDistributionVector::GetRange(FVector& OutMin, FVector& OutMax) const
{
	FVector A;
	FVector B;

	switch (Type)
	{
	case ERawDistributionType::Uniform:
	{
		GetUniformBounds(A, B);

		// 잠긴 축은 복사해 오는 축과 범위가 같다
		switch (LockedAxes)
		{
		case EDistributionVectorLockFlags::EDVLF_XY:  A.Y = A.X; B.Y = B.X; break;
		case EDistributionVectorLockFlags::EDVLF_XZ:  A.Z = A.X; B.Z = B.X; break;
		case EDistributionVectorLockFlags::EDVLF_YZ:  A.Z = A.Y; B.Z = B.Y; break;
		case EDistributionVectorLockFlags::EDVLF_XYZ: A.Y = A.X; A.Z = A.X; B.Y = B.X; B.Z = B.X; break;
		default: break;
		}
		break;
	}

	case ERawDistributionType::ConstantCurve:
		A = CurveStart;
		B = CurveEnd;
		break;

	case ERawDistributionType::Constant:
	default:
		A = Constant;
		B = Constant;
		break;
	}

	// Min이 Max보다 크게 입력돼 있어도 작은 쪽이 OutMin이 되게 한다
	OutMin = FVector(std::min(A.X, B.X), std::min(A.Y, B.Y), std::min(A.Z, B.Z));
	OutMax = FVector(std::max(A.X, B.X), std::max(A.Y, B.Y), std::max(A.Z, B.Z));
}

void FRawDistributionVector::GetOutRange(float& OutMin, float& OutMax) const
{
	FVector RangeMin;
	FVector RangeMax;
	GetRange(RangeMin, RangeMax);

	OutMin = std::min({ RangeMin.X, RangeMin.Y, RangeMin.Z });
	OutMax = std::max({ RangeMax.X, RangeMax.Y, RangeMax.Z });
}
