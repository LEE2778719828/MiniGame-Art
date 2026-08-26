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

/** Runtime Night collection amount. Fractional amounts are quantized only at the Night -> Day boundary. */
USTRUCT(BlueprintType)
struct FIngredientFloatStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	EIngredientId Id = EIngredientId::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night")
	float Amount = 0.f;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Gift")
	float PreForkGatherAmountBonus = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Gift")
	int32 MatchShieldCharges = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Gift")
	float PostForkInvulnerableSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Gift")
	float NearDeathHealAmount = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Gift")
	float NearDeathThreshold = 0.f;
};

/** S -> R2 : start night once. */
USTRUCT(BlueprintType)
struct FNightBootstrap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night", meta = (DisplayName = "Night关卡ID", ToolTip = "Day 传入的 Night 关卡标识，例如 T0/L1/L2/L3。"))
	ENightLevelId LevelId = ENightLevelId::T0;

	/** Day-selected route mode used for the pre-fork main segment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night", meta = (DisplayName = "默认路线模式", ToolTip = "由 Day 关卡传入的岔路前主段模式；可选 A/B/C，默认 A。它不会跳过 ForkChoice。"))
	ENightRouteId DefaultRoute = ENightRouteId::A;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night", meta = (DisplayName = "岔路组合", ToolTip = "Day 传入的特殊岔路组合：AB、AC 或 BC；默认 AB。"))
	ENightForkPair ForkPair = ENightForkPair::AB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night", meta = (DisplayName = "Gift效果状态", ToolTip = "Day 传入的 Gift/Buff 状态；通常由 Day 流程自动填写。"))
	FGiftBuffState GiftBuffs;

	/** Optional weighted foe pool. Repeated IDs represent higher weight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night", meta = (DisplayName = "敌人权重覆盖", ToolTip = "可选的运行时敌人权重池；重复 ID 提高概率。为空时使用 DA_Course.FoeWeightPool。"))
	TArray<EFoeId> FoeWeightOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night", meta = (DisplayName = "Night随机种子", ToolTip = "课程随机种子；Day 表通常通过 ReviewSeed 传入。"))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (DisplayName = "触发后分支节拍数", ToolTip = "完成多少个分支节拍后触发 KeySwap。"))
	int32 TriggerAfterBranchBeats = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (DisplayName = "KeySwap警告时间", ToolTip = "切换前警告持续时间，单位秒。"))
	float WarningSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (DisplayName = "KeySwap安全停拍时间", ToolTip = "切换后暂停开拍的时间，单位秒。"))
	float SafetyHoldSeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (DisplayName = "是否切换", ToolTip = "开启后将当前控制方案切换到 TargetScheme；关闭仅显示提示。"))
	bool bToggle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Proc", meta = (DisplayName = "目标控制方案", ToolTip = "KeySwap 生效后的控制方案。"))
	ENightControlScheme TargetScheme = ENightControlScheme::Swapped;
};
#pragma endregion K2 moonyfli
