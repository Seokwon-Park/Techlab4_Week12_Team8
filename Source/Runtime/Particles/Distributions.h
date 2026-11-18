#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Math/Vector.h"
#include "Particles/ParticleCurve.h"
#include "Distributions.generated.h"

// 분포 종류
UENUM()
enum class ERawDistributionType : uint8
{
	Constant,       // 상수
	Uniform,        // Min~Max 랜덤값
	ConstantCurve,  // 입력을 0~1로 자른 후 보간
	Curve,
};

// Uniform 벡터에서 한 축의 값을 다른 축에 그대로 쓴다
UENUM()
enum class EDistributionVectorLockFlags : uint8
{
	EDVLF_None,     // 축마다 따로 뽑는다
	EDVLF_XY,       // Y = X
	EDVLF_XZ,       // Z = X
	EDVLF_YZ,       // Z = Y
	EDVLF_XYZ,      // Y = Z = X (크기를 가로세로 같은 비율로 키울 때)
	EDVLF_MAX UMETA(Hidden)
};

// Uniform 벡터에서 축마다 Min을 어떻게 정할지
UENUM()
enum class EDistributionVectorMirrorFlags : uint8
{
	EDVMF_Same,       // Min을 무시하고 Max로 고정한다
	EDVMF_Different,  // Min~Max를 그대로 쓴다 (기본)
	EDVMF_Mirror,     // Min = -Max. 0을 중심으로 대칭인 범위가 된다
	EDVMF_MAX UMETA(Hidden)
};

USTRUCT()
struct FRawDistributionFloat
{
	GENERATED_BODY()

public:
	FRawDistributionFloat() = default;

	static FRawDistributionFloat MakeConstant(float Value);
	static FRawDistributionFloat MakeUniform(float InMin, float InMax);
	static FRawDistributionFloat MakeConstantCurve(float StartValue, float EndValue);

	// F: 시간 입력
	float GetValue(float F = 0.0f) const;

	// 이 분포가 낼 수 있는 가장 작은 값과 가장 큰 값
	void GetOutRange(float& OutMin, float& OutMax) const;

	ERawDistributionType GetType() const { return Type; }

	// 추가: 키가 없으면 Constant를 첫 키로 삼는다 (커브 에디터가 트랙을 열 때 부른다)
	void EnsureCurveKeys();

	// 추가: Curve 타입의 키. UPROPERTY가 아니다 (구조체 안 TArray는 리플렉션이 저장 못 함).
	// 저장은 11단계가, 편집은 12단계 커브 에디터가 한다
	FParticleCurve Curve;

private:
	UPROPERTY(EditAnywhere)
	ERawDistributionType Type = ERawDistributionType::Constant;

	// Constant
	UPROPERTY(EditAnywhere)
	float Constant = 0.0f;

	// Uniform
	UPROPERTY(EditAnywhere)
	float Min = 0.0f;

	UPROPERTY(EditAnywhere)
	float Max = 0.0f;

	// Constant Curve
	UPROPERTY(EditAnywhere)
	float CurveStart = 0.0f;

	UPROPERTY(EditAnywhere)
	float CurveEnd = 0.0f;
};

USTRUCT()
struct FRawDistributionVector
{
	GENERATED_BODY()

public:
	FRawDistributionVector() = default;

	static FRawDistributionVector MakeConstant(const FVector& Value);
	static FRawDistributionVector MakeUniform(
		const FVector& InMin, const FVector& InMax,
		EDistributionVectorLockFlags InLockedAxes = EDistributionVectorLockFlags::EDVLF_None
	);
	static FRawDistributionVector MakeConstantCurve(const FVector& StartValue, const FVector& EndValue);

	// F: 시간 입력
	FVector GetValue(float F = 0.0f) const;

	// 축마다 낼 수 있는 가장 작은 값과 가장 큰 값
	void GetRange(FVector& OutMin, FVector& OutMax) const;

	// 모든 축을 통틀어 가장 작은 값과 가장 큰 값
	void GetOutRange(float& OutMin, float& OutMax) const;

	ERawDistributionType GetType() const { return Type; }

	void EnsureCurveKeys();

	// 축마다 독립 커브 (X, Y, Z). 색 분포면 R, G, B
	FParticleCurve Curves[3];

private:
	// Uniform에서 Mirror Flags를 적용한 축별 Min / Max
	void GetUniformBounds(FVector& OutMin, FVector& OutMax) const;

	UPROPERTY(EditAnywhere)
	ERawDistributionType Type = ERawDistributionType::Constant;

	UPROPERTY(EditAnywhere)
	FVector Constant = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere)
	FVector Min = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere)
	FVector Max = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere)
	EDistributionVectorLockFlags LockedAxes = EDistributionVectorLockFlags::EDVLF_None;   // Uniform

	// 배열 UPROPERTY가 지원되지 않아 축 별로
	UPROPERTY(EditAnywhere)
	EDistributionVectorMirrorFlags MirrorX = EDistributionVectorMirrorFlags::EDVMF_Different;

	UPROPERTY(EditAnywhere)
	EDistributionVectorMirrorFlags MirrorY = EDistributionVectorMirrorFlags::EDVMF_Different;

	UPROPERTY(EditAnywhere)
	EDistributionVectorMirrorFlags MirrorZ = EDistributionVectorMirrorFlags::EDVMF_Different;

	UPROPERTY(EditAnywhere)
	FVector CurveStart = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere)
	FVector CurveEnd = FVector(0.0f, 0.0f, 0.0f);
};
