#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Night/Course/NightFeelBridge.h"
#include "NightFeelStubComponent.generated.h"

class UInputAction;
class UInputMappingContext;
class UNightFeelTuningData; //add by K2
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNightFeelDebug, float, Soul, ENightJudgeOutcome, LastOutcome);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNightFeelInputResolved, int32, NodeIndex, ENightJudgeOutcome, Outcome);

// add by K2 (R1)
/**
 * Feel 判定阶段。
 * Idle       = 石间移动中（没有开窗，此时的输入算「提前」，进缓存并加速衔接）
 * WindowOpen = 已落地，处在空档期宽限内（按对成功、按错扣魂、不按不罚）
 * Breathing  = 空档期已过仍未行动，按 BreathDecayPerSecond 持续掉魂（裁定 R-006）
 */
UENUM(BlueprintType)
enum class ENightFeelPhase : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	WindowOpen UMETA(DisplayName = "WindowOpen"),
	Breathing UMETA(DisplayName = "Breathing")
};

#pragma region K2 moonyfli
/**
 * Built-in R1 Feel stub for G1 PIE.
 * Correct: Attack on Enemy, Jump on Hazard.
 */
UCLASS(ClassGroup = (Night), meta = (BlueprintSpawnableComponent))
class MINIGAME_API UNightFeelStubComponent : public UActorComponent, public INightFeelBridge
{
	GENERATED_BODY()

public:
	UNightFeelStubComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Feel|Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Feel|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Feel|Input")
	TObjectPtr<UInputAction> AttackAction;

	// add by K2 (R1) —— 手感参数表
	/**
	 * 挂上后 BeginPlay 会用表里的值覆盖下面所有手感参数，让调参集中在一个资产里。
	 * 留空则用组件上的默认值（旧行为）。运行中改表用 Night.Feel.Reload 重新套用。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Tuning")
	TObjectPtr<UNightFeelTuningData> Tuning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel")
	float Soul = 100.f;

#pragma region K2 moonyfli
	/** Successful slash +1; miss / wrong button clears. Jump success does not change Combo. */
	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel")
	int32 Combo = 0;

	/** Highest Combo this Night. Survives a miss; reset when the Night binds the player. */
	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel")
	int32 MaxCombo = 0;
#pragma endregion K2 moonyfli

	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel")
	FNightJudgeRequest ActiveRequest;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel")
	bool bHasActiveRequest = false;

	UPROPERTY(BlueprintAssignable, Category = "Night|Feel|Debug")
	FOnNightFeelDebug OnDebugSoulChanged;

	UPROPERTY(BlueprintAssignable, Category = "Night|Feel")
	FOnNightFeelInputResolved OnInputResolved;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel|Debug")
	ENightJudgeOutcome LastOutcome = ENightJudgeOutcome::None;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel")
	ENightControlScheme ControlScheme = ENightControlScheme::Normal;

	// add by K2 (R1) —— 空档期（策划案表 3-6；裁定 R-006：过期不再是失误，只开始呼吸扣血）
	/**
	 * 落地后的「空档期」宽限（ms）：这段时间内不行动不受罚。
	 * 跳跃节点用此值。窗口本身永不关闭，按对随时算命中。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Judge", meta = (ClampMin = "0.0"))
	float JumpWindowMs = 280.f;

	/** 斩击节点的空档期宽限（ms）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Judge", meta = (ClampMin = "0.0"))
	float AttackWindowMs = 260.f;

	/** T0 冻结式教学的放宽档（§7.1）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Judge")
	bool bUseTutorialWindows = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Judge", meta = (ClampMin = "0.0"))
	float TutorialJumpWindowMs = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Judge", meta = (ClampMin = "0.0"))
	float TutorialAttackWindowMs = 520.f;

	// add by K2 (R1) —— 呼吸扣血（裁定 R-006）
	/**
	 * add by K2 (R1) 裁定 R-007：空档期从上一个动作播完之后才起算。
	 * 关掉则退回旧口径（落地即起算），仅用于对比。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Soul")
	bool bGraceWaitsForAnim = true;

	/** 上一拍等了多久动画收尾（秒），仅调试可视。 */
	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel|Debug")
	float LastAnimTailSeconds = 0.f;

	/** 是否启用呼吸扣血（关掉则发呆完全免费，仅用于调试）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Soul")
	bool bEnableBreathDecay = true;

	/**
	 * 空档期过后每秒掉多少魂，直到玩家行动。
	 * 与「按错扣魂」分开取值：按错是失误、要痛；发呆是拖沓、温和但持续。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Soul", meta = (ClampMin = "0.0"))
	float BreathDecayPerSecond = 0.55f;

	// add by K2 (R1) —— 按错扣魂（裁定 R-002：按节点类型取值，不按失误类型取值）
	/** 障碍（跳跃节点）失误扣魂。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Soul", meta = (ClampMin = "0.0"))
	float SoulPenaltyHazard = 7.f;

	/** 怪物（劈砍节点）失误扣魂。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Soul", meta = (ClampMin = "0.0"))
	float SoulPenaltyEnemy = 5.f;

	/** 受击后无敌（ms）：期间不再重复扣魂，防止连按被连罚。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Soul", meta = (ClampMin = "0.0"))
	float HitInvulnMs = 600.f;

	// add by K2 (R1) —— 负反应缓存 + 加速追赶
	/** 早于空档这么多 ms 内的输入进缓存；更早则空按忽略（不罚血、不断连）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|CatchUp", meta = (ClampMin = "0.0"))
	float EarlyAcceptMs = 550.f;

	/** 缓存到输入时，未走完的石间移动按此速率加速播完。1 = 不加速。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|CatchUp", meta = (ClampMin = "1.0"))
	float CatchUpPlayRate = 1.5f;

	/** 加速最多吃掉的提前量（ms），超出不再压缩。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|CatchUp", meta = (ClampMin = "0.0"))
	float MaxCatchUpCompressMs = 300.f;

	/** 缓存队列容量（连招地标的白盒近似）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|CatchUp", meta = (ClampMin = "1"))
	int32 MaxBufferedInputs = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Debug")
	bool bLogJudge = true;

	/** add by K2 (R1)：屏显文字同时写日志（屏显变化太快看不清时用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Feel|Debug")
	bool bLogHudLines = true;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel|Debug")
	ENightFeelPhase Phase = ENightFeelPhase::Idle;

	/** 当前空档剩余秒数（仅调试/表现用）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel|Debug")
	float WindowRemainingSeconds = 0.f;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 本拍应有的空档宽度（秒）。 */
	UFUNCTION(BlueprintPure, Category = "Night|Feel|Judge")
	float GetWindowSeconds(ENightNodeKind Kind) const;

	UFUNCTION(BlueprintCallable, Category = "Night|Feel")
	void SetupInput(APlayerController* PC);

	/** add by K2 (R1)：套用 Tuning 表。没挂表返回 false。 */
	UFUNCTION(BlueprintCallable, Category = "Night|Feel|Tuning")
	bool ApplyTuning();

	virtual void NotifyJudgeRequest_Implementation(const FNightJudgeRequest& Request) override;
	virtual void ClearJudgeRequest_Implementation(int32 NodeIndex) override;
	virtual ENightJudgeOutcome TryResolveInput_Implementation(ENightFeelInput Input) override;
	virtual float GetSoul_Implementation() const override;
	virtual void ApplySoulPenalty_Implementation(float Amount, ENightJudgeOutcome Reason) override;
	virtual void RestoreSoul_Implementation(float Amount, float MaxSoul) override;
	virtual void PlaySuccessFeedback_Implementation(ENightNodeKind Kind) override;
	virtual void PlayFailFeedback_Implementation(ENightJudgeOutcome Outcome, ENightNodeKind Kind) override;
	virtual void SetControlScheme_Implementation(ENightControlScheme Scheme) override;
	virtual ENightControlScheme GetControlScheme_Implementation() const override;

	void HandleJump(const FInputActionValue& Value);
	void HandleAttack(const FInputActionValue& Value);

protected:
	ENightFeelInput RemapInput(ENightFeelInput Input) const;

	// add by K2 (R1)
	struct FNightFeelBufferedInput
	{
		ENightFeelInput Input = ENightFeelInput::Jump;
		float WorldTime = 0.f;
	};

	float GetWorldTimeSeconds() const;

	/** 主角当前动作还剩多久（秒）；拿不到 Pawn 或没在播返回 0。 */
	float GetHeroActionRemainingSeconds() const;

	ENightJudgeOutcome JudgeInput(ENightFeelInput Input);

	/** 空档期结束、转入呼吸扣血（只在转入的那一下调一次）。 */
	void BeginBreathing();

	/** 呼吸扣血的每帧结算。 */
	void TickBreathDecay(float DeltaTime);

	void ApplyFailurePenalty(ENightJudgeOutcome Reason);
	void PruneBufferedInputs();
	bool TryConsumeBufferedInput();
	void RequestCatchUp();
	void ShowWindowDebug(ENightNodeKind Kind, float WindowSeconds) const;

	/** 入缓存；bAllowCatchUp 仅在石间移动中为 true（僵直期没有移动可加速）。 */
	bool BufferInput(ENightFeelInput Input, bool bAllowCatchUp, const TCHAR* ReasonTag);

	/** 屏显 + 日志同一份文字，便于回溯屏上一闪而过的提示。 */
	void PushHudLine(int32 Key, float Duration, const FColor& Color, const FString& Msg) const;

	/** 屏显里该提示哪个键，随换键状态变化，避免换键后屏显撒谎。 */
	const TCHAR* GetInputHintText(bool bAttack) const;

	TArray<FNightFeelBufferedInput> BufferedInputs;
	float WindowOpenWorldTime = 0.f;
	float WindowCloseWorldTime = 0.f;
	float InvulnEndWorldTime = 0.f;

	/** 呼吸扣血的日志节流用（累计到 1 点魂再打一行）。 */
	float BreathLoggedAccum = 0.f;

	/** 本节点的空档期是否已过（已转入呼吸扣血）。窗口不会因此关闭。 */
	UPROPERTY(BlueprintReadOnly, Category = "Night|Feel|Debug")
	bool bBreathing = false;
};
#pragma endregion K2 moonyfli
