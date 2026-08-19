#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NightFeelTuningData.generated.h"

class UNightFeelStubComponent;
class ANightCoursePawn;

#pragma region K2 moonyfli
/**
 * R1 手感参数表。判定窗、空档期、缓存加速、动作倍率集中一处，可以存多套预设互相对比。
 *
 * 用法：挂到 FeelStub 的 Tuning 上，BeginPlay 时套用；PIE 里用 Night.Feel.Set 现调，
 * 满意了 Night.Feel.Save 写回本表。改完表不重开 PIE 的话，用 Night.Feel.Reload。
 *
 * 边界（重要）：StartingSoul 与 WrongPenalty / MissPenalty 归 R2 的 UNightG1CourseConfig，
 * 运行时由 Host 和 Director 直接写进来，本表**不重复定义**——两个表管同一个值迟早出事。
 */
UCLASS(BlueprintType)
class MINIGAME_API UNightFeelTuningData : public UDataAsset
{
	GENERATED_BODY()

public:
	// ---------------------------------------------------------------- 空档期
	/** 落地后不行动也不受罚的宽限（ms）。窗口本身永不关闭，按对随时命中。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grace 空档期", meta = (ClampMin = "0.0"))
	float JumpWindowMs = 280.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grace 空档期", meta = (ClampMin = "0.0"))
	float AttackWindowMs = 260.f;

	/** 裁定 R-007：空档期从上一个动作播完之后才起算。关掉退回「落地即起算」。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grace 空档期")
	bool bGraceWaitsForAnim = true;

	/** T0 冻结式教学的放宽档。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grace 空档期")
	bool bUseTutorialWindows = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grace 空档期", meta = (ClampMin = "0.0", EditCondition = "bUseTutorialWindows"))
	float TutorialJumpWindowMs = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grace 空档期", meta = (ClampMin = "0.0", EditCondition = "bUseTutorialWindows"))
	float TutorialAttackWindowMs = 520.f;

	// ------------------------------------------------------ 提前容忍与追赶加速
	/** 早于空档这么多 ms 内的输入进缓存；更早则忽略（不罚血、不断连）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CatchUp 提前与追赶", meta = (ClampMin = "0.0"))
	float EarlyAcceptMs = 550.f;

	/** 命中缓存时，未走完的石间移动按此速率加速播完。1 = 不加速。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CatchUp 提前与追赶", meta = (ClampMin = "1.0"))
	float CatchUpPlayRate = 1.5f;

	/** 加速最多吃掉的提前量（ms），超出不再压缩。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CatchUp 提前与追赶", meta = (ClampMin = "0.0"))
	float MaxCatchUpCompressMs = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CatchUp 提前与追赶", meta = (ClampMin = "1"))
	int32 MaxBufferedInputs = 3;

	// -------------------------------------------------------------------- 扣魂
	/** 空档期过后每秒掉多少魂，直到玩家行动（裁定 R-006 呼吸扣血）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soul 扣魂", meta = (ClampMin = "0.0"))
	float BreathDecayPerSecond = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soul 扣魂")
	bool bEnableBreathDecay = true;

	/**
	 * 按错扣魂，按节点类型取值。
	 * 注意：Director 另有一条 Config->WrongPenalty / MissPenalty 的路径，两套数字覆盖重叠情形，
	 * 待与 R2 收口。改这里只影响 FeelStub 自判的那条路。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soul 扣魂", meta = (ClampMin = "0.0"))
	float SoulPenaltyHazard = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soul 扣魂", meta = (ClampMin = "0.0"))
	float SoulPenaltyEnemy = 5.f;

	/** 受击后无敌（ms）：期间不重复扣魂，防止连按被连罚。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soul 扣魂", meta = (ClampMin = "0.0"))
	float HitInvulnMs = 600.f;

	// -------------------------------------------------------------------- 动作
	/** 动作基准播放倍率（<1 变慢、>1 变快）。追赶加速在此基础上再乘。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim 动作", meta = (ClampMin = "0.05", ClampMax = "4.0"))
	float JumpAnimRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim 动作", meta = (ClampMin = "0.05", ClampMax = "4.0"))
	float AttackAnimRate = 1.f;

	/** 开启后石间位移时长由动画锚点决定，而不是 R2 配的 AdvanceSpeed。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim 动作")
	bool bAnimDrivenAdvance = false;

	/** 位移与动作的对齐锚点（ms，倍率 1.0 下）：跳跃取落地帧、斩击取接触帧。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim 动作", meta = (ClampMin = "10.0", EditCondition = "bAnimDrivenAdvance"))
	float JumpAnchorMs = 266.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim 动作", meta = (ClampMin = "10.0", EditCondition = "bAnimDrivenAdvance"))
	float AttackAnchorMs = 179.f;

	// -------------------------------------------------------------------- 调试
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug 调试")
	bool bLogJudge = true;

	/** 屏显文字同时写日志（屏显一闪而过看不清时用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug 调试")
	bool bLogHudLines = true;

	/** 把本表的值套到运行中的 Feel 组件与主角上。Pawn 可为空（只套 Feel 部分）。 */
	void ApplyTo(UNightFeelStubComponent& Feel, ANightCoursePawn* Pawn) const;

	/** 反向：把运行中的当前值收回本表，供 Night.Feel.Save 落档。 */
	void CaptureFrom(const UNightFeelStubComponent& Feel, const ANightCoursePawn* Pawn);
};
#pragma endregion K2 moonyfli
