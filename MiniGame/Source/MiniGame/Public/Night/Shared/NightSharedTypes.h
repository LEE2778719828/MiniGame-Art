#pragma once

#include "CoreMinimal.h"
#include "NightSharedTypes.generated.h"

#pragma region K2 moonyfli
UENUM(BlueprintType)
enum class ENightLevelId : uint8
{
	T0 UMETA(DisplayName = "T0"),
	L1 UMETA(DisplayName = "L1"),
	L2 UMETA(DisplayName = "L2"),
	L3 UMETA(DisplayName = "L3")
};

UENUM(BlueprintType)
enum class ENightRouteId : uint8
{
	None UMETA(DisplayName = "None"),
	A UMETA(DisplayName = "A"),
	B UMETA(DisplayName = "B"),
	C UMETA(DisplayName = "C")
};

UENUM(BlueprintType)
enum class ENightForkPair : uint8
{
	AB UMETA(DisplayName = "AB"),
	AC UMETA(DisplayName = "AC"),
	BC UMETA(DisplayName = "BC")
};

UENUM(BlueprintType)
enum class ENightControlScheme : uint8
{
	Normal UMETA(DisplayName = "Normal"),
	Swapped UMETA(DisplayName = "Swapped")
};

UENUM(BlueprintType)
enum class ENightForkEnv : uint8
{
	ClearAB UMETA(DisplayName = "Clear AB"),
	FogAC UMETA(DisplayName = "Fog AC"),
	ReverseBC UMETA(DisplayName = "Reverse BC"),
	Custom UMETA(DisplayName = "Custom")
};

UENUM(BlueprintType)
enum class EIngredientId : uint8
{
	None UMETA(DisplayName = "None"),
	F01_LingGu UMETA(DisplayName = "LingGu"),
	F02_YinShanJun UMETA(DisplayName = "YinShanJun"),
	F03_ChiYanJiao UMETA(DisplayName = "ChiYanJiao"),
	F04_YueLinYu UMETA(DisplayName = "YueLinYu"),
	F05_XuanYuQin UMETA(DisplayName = "XuanYuQin")
};

UENUM(BlueprintType)
enum class EFoeId : uint8
{
	None UMETA(DisplayName = "None"),
	M01 UMETA(DisplayName = "M01"),
	M02 UMETA(DisplayName = "M02"),
	M03 UMETA(DisplayName = "M03"),
	M04 UMETA(DisplayName = "M04"),
	M05 UMETA(DisplayName = "M05")
};

USTRUCT(BlueprintType)
struct FIngredientStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	EIngredientId Id = EIngredientId::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	int32 Count = 0;
};

USTRUCT(BlueprintType)
struct FGiftBuffState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Gift")
	bool bGuideKite = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Gift")
	bool bSpareLamp = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Gift")
	bool bKeyCoin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Gift")
	bool bTaotieBox = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Gift")
	EIngredientId TaotieLockIngredient = EIngredientId::None;
};

/** S -> R2 : start night once. */
USTRUCT(BlueprintType)
struct FNightBootstrap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	ENightLevelId LevelId = ENightLevelId::T0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	ENightForkPair ForkPair = ENightForkPair::AB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	FGiftBuffState GiftBuffs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	int32 Seed = 1001;
};

/** R2 -> S : finish night once. */
USTRUCT(BlueprintType)
struct FNightResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	bool bSuccess = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	bool bFailedMidway = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	ENightRouteId RouteTaken = ENightRouteId::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	TArray<FIngredientStack> Ingredients;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	float SoulLeft = 0.f;
};

USTRUCT(BlueprintType)
struct FNightKeySwapCue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	int32 TriggerAfterBranchBeats = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	float WarningSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	float SafetyHoldSeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	bool bToggle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc")
	ENightControlScheme TargetScheme = ENightControlScheme::Swapped;
};
#pragma endregion K2 moonyfli
