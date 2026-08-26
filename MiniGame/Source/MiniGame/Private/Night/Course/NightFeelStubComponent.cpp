#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightFeelTuningData.h" //add by K2
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#pragma region K2 moonyfli
UNightFeelStubComponent::UNightFeelStubComponent()
{
	// add by K2 (R1): 空档超时判 Miss、缓存过期、僵直退出都靠 Tick 推进
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UNightFeelStubComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplyTuning(); //add by K2

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		SetupInput(Cast<APlayerController>(Pawn->GetController()));
	}
}

// add by K2 (R1)
bool UNightFeelStubComponent::ApplyTuning()
{
	if (!Tuning)
	{
		return false;
	}

	Tuning->ApplyTo(*this, Cast<ANightCoursePawn>(GetOwner()));
	UE_LOG(LogTemp, Log, TEXT("[NightFeel] 已套用手感表 %s"), *Tuning->GetName());
	return true;
}

void UNightFeelStubComponent::SetupInput(APlayerController* PC)
{
	if (!PC)
	{
		return;
	}

	// Mapping only here. Action binds live on the Pawn to avoid double-fire.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		if (MappingContext)
		{
			Subsystem->AddMappingContext(MappingContext, 1);
		}
	}
}

// add by K2 (R1)
void UNightFeelStubComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float Now = GetWorldTimeSeconds();

	PruneBufferedInputs();

	if (!bHasActiveRequest)
	{
		WindowRemainingSeconds = 0.f;
		return;
	}

	WindowRemainingSeconds = FMath::Max(0.f, WindowCloseWorldTime - Now);

	// 裁定 R-006：空档期过了不算失误，只是从这一刻开始"呼吸"掉魂，窗口不关
	if (!bBreathing && Now > WindowCloseWorldTime)
	{
		BeginBreathing();
	}
	if (bBreathing)
	{
		TickBreathDecay(DeltaTime);
	}

	TryConsumeBufferedInput();
}

// add by K2 (R1)
float UNightFeelStubComponent::GetWindowSeconds(ENightNodeKind Kind) const
{
	const bool bAttack = (Kind == ENightNodeKind::Enemy);
	const float Ms = bUseTutorialWindows
		? (bAttack ? TutorialAttackWindowMs : TutorialJumpWindowMs)
		: (bAttack ? AttackWindowMs : JumpWindowMs);
	return FMath::Max(0.f, Ms) * 0.001f;
}

float UNightFeelStubComponent::GetWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

// add by K2 (R1)
float UNightFeelStubComponent::GetHeroActionRemainingSeconds() const
{
	const ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(GetOwner());
	return CoursePawn ? CoursePawn->GetHeroActionRemainingSeconds() : 0.f;
}

void UNightFeelStubComponent::NotifyJudgeRequest_Implementation(const FNightJudgeRequest& Request)
{
	ActiveRequest = Request;
	bHasActiveRequest = true;

	// add by K2 (R1): 输入窗从「到石那一刻」起算，此刻起按对即命中。
	// 但空档期（挂机扣血的起算点）按裁定 R-007 要等上一个动作播完 —— 动画还在播，
	// 角色就还在执行，这时候判玩家发呆是拿动画长度惩罚他。
	const float Now = GetWorldTimeSeconds();
	const float WindowSeconds = GetWindowSeconds(Request.Kind);
	const float AnimTail = bGraceWaitsForAnim ? GetHeroActionRemainingSeconds() : 0.f;
	WindowOpenWorldTime = Now;
	WindowCloseWorldTime = Now + AnimTail + WindowSeconds;
	WindowRemainingSeconds = AnimTail + WindowSeconds;
	LastAnimTailSeconds = AnimTail;
	bBreathing = false;
	BreathLoggedAccum = 0.f;
	Phase = ENightFeelPhase::WindowOpen;

	ShowWindowDebug(Request.Kind, AnimTail + WindowSeconds);

	// 负反应缓存留给下一帧 Tick 兑现：此处仍在 Director 的 TryOpenBeat 调用栈里，
	// 立即结算会再入它的 ResolveBeat。
	PruneBufferedInputs();
}

void UNightFeelStubComponent::ClearJudgeRequest_Implementation(int32 NodeIndex)
{
	if (bHasActiveRequest && ActiveRequest.NodeIndex == NodeIndex)
	{
		bHasActiveRequest = false;
		ActiveRequest = FNightJudgeRequest();
		WindowRemainingSeconds = 0.f;
		bBreathing = false;
		BreathLoggedAccum = 0.f;
		Phase = ENightFeelPhase::Idle;
		PushHudLine(9911, 0.05f, FColor::Black, FString());
	}
}

ENightFeelInput UNightFeelStubComponent::RemapInput(ENightFeelInput Input) const
{
	if (ControlScheme != ENightControlScheme::Swapped)
	{
		return Input;
	}
	return (Input == ENightFeelInput::Jump) ? ENightFeelInput::Attack : ENightFeelInput::Jump;
}

ENightJudgeOutcome UNightFeelStubComponent::TryResolveInput_Implementation(ENightFeelInput Input)
{
	// 裁定 R-002/R-006：窗口永不关闭，按对随时算命中；按错只扣魂、不锁输入（僵直已取消）
	if (bHasActiveRequest)
	{
		return JudgeInput(Input);
	}

	// add by K2 (R1): 无开窗（石间移动中）= 负反应区，入缓存并加速追赶
	BufferInput(Input, /*bAllowCatchUp=*/true, TEXT("early"));
	return ENightJudgeOutcome::None;
}

// add by K2 (R1)
bool UNightFeelStubComponent::BufferInput(ENightFeelInput Input, bool bAllowCatchUp, const TCHAR* ReasonTag)
{
	PruneBufferedInputs();
	if (BufferedInputs.Num() >= FMath::Max(1, MaxBufferedInputs))
	{
		if (bLogJudge)
		{
			UE_LOG(LogTemp, Log, TEXT("[NightFeel] buffer full (%d), input dropped [%s]"),
				BufferedInputs.Num(), ReasonTag);
		}
		return false;
	}

	FNightFeelBufferedInput Pending;
	Pending.Input = Input;
	Pending.WorldTime = GetWorldTimeSeconds();
	BufferedInputs.Add(Pending);

	if (bAllowCatchUp)
	{
		RequestCatchUp();
	}
	if (bLogJudge)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightFeel] buffered %s input (%d in queue) [%s]"),
			Input == ENightFeelInput::Attack ? TEXT("ATTACK") : TEXT("JUMP"),
			BufferedInputs.Num(), ReasonTag);
	}
	return true;
}

// add by K2 (R1)
// 裁定 R-002：只有按对才广播（推进）；按错在本地结算，原地等玩家改口
ENightJudgeOutcome UNightFeelStubComponent::JudgeInput(ENightFeelInput Input)
{
	// 换键只在判定这一处映射：缓存里存原始输入，避免入队和出队各映射一次
	const ENightFeelInput Effective = RemapInput(Input);
	const bool bExpectAttack = (ActiveRequest.Kind == ENightNodeKind::Enemy);
	const bool bExpectJump = (ActiveRequest.Kind == ENightNodeKind::Hazard);
	const bool bCorrect =
		(bExpectAttack && Effective == ENightFeelInput::Attack) ||
		(bExpectJump && Effective == ENightFeelInput::Jump);

	LastOutcome = bCorrect ? ENightJudgeOutcome::Success : ENightJudgeOutcome::WrongButton;
	const int32 ResolvedIndex = ActiveRequest.NodeIndex;

	if (bLogJudge)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightFeel] idx=%d %s (%+.0fms vs %.0fms 空档%s)"),
			ResolvedIndex,
			bCorrect ? TEXT("Success") : TEXT("WrongButton"),
			(GetWorldTimeSeconds() - WindowOpenWorldTime) * 1000.f,
			GetWindowSeconds(ActiveRequest.Kind) * 1000.f,
			bBreathing ? TEXT(", breathing") : TEXT(""));
	}

	if (bCorrect)
	{
		OnDebugSoulChanged.Broadcast(Soul, LastOutcome);
		OnInputResolved.Broadcast(ResolvedIndex, LastOutcome);
		return LastOutcome;
	}

	// CourseDirector owns the route-scaled penalty and feedback. Broadcast the
	// failed attempt while keeping the request open so a corrected input can
	// still resolve the same beat.
	OnInputResolved.Broadcast(ResolvedIndex, LastOutcome);
	return LastOutcome;
}

// add by K2 (R1)
// 裁定 R-006：空档期过了不算失误（不扣固定值、不广播 Miss、不断连），转入呼吸扣血
void UNightFeelStubComponent::BeginBreathing()
{
	bBreathing = true;
	BreathLoggedAccum = 0.f;
	Phase = ENightFeelPhase::Breathing;

	if (bLogJudge)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightFeel] idx=%d 空档期结束（等动画 %.0fms + 宽限 %.0fms），开始呼吸扣血 %.2f/s"),
			ActiveRequest.NodeIndex, LastAnimTailSeconds * 1000.f,
			GetWindowSeconds(ActiveRequest.Kind) * 1000.f, BreathDecayPerSecond);
	}

	const bool bAttack = (ActiveRequest.Kind == ENightNodeKind::Enemy);
	PushHudLine(9911, 999.f, FColor::Yellow, FString::Printf(
		TEXT("BREATHING (-%.2f/s): still need %s  (idx=%d)"),
		BreathDecayPerSecond,
		GetInputHintText(bAttack),
		ActiveRequest.NodeIndex));
}

// add by K2 (R1)
void UNightFeelStubComponent::TickBreathDecay(float DeltaTime)
{
	if (!bEnableBreathDecay || BreathDecayPerSecond <= 0.f || Soul <= 0.f)
	{
		return;
	}

	const float Loss = BreathDecayPerSecond * DeltaTime;
	Soul = FMath::Max(0.f, Soul - Loss);
	OnDebugSoulChanged.Broadcast(Soul, LastOutcome);

	// 日志按「累计掉满 1 点」节流，否则每帧一行没法看
	BreathLoggedAccum += Loss;
	if (bLogJudge && BreathLoggedAccum >= 1.f)
	{
		BreathLoggedAccum -= 1.f;
		UE_LOG(LogTemp, Log, TEXT("[NightFeel] 呼吸扣血 -> %.1f (idx=%d 已发呆 %.1fs)"),
			Soul, ActiveRequest.NodeIndex, GetWorldTimeSeconds() - WindowCloseWorldTime);
	}
}

// add by K2 (R1)
// 扣魂按节点类型取值（裁定 R-002），并受 0.6s 无敌节流
void UNightFeelStubComponent::ApplyFailurePenalty(ENightJudgeOutcome Reason)
{
	const float Now = GetWorldTimeSeconds();
	if (Now < InvulnEndWorldTime)
	{
		if (bLogJudge)
		{
			UE_LOG(LogTemp, Log, TEXT("[NightFeel] penalty skipped: invulnerable for %.0fms more"),
				(InvulnEndWorldTime - Now) * 1000.f);
		}
		return;
	}
	InvulnEndWorldTime = Now + FMath::Max(0.f, HitInvulnMs) * 0.001f;

	const float Amount = (ActiveRequest.Kind == ENightNodeKind::Enemy) ? SoulPenaltyEnemy : SoulPenaltyHazard;
	Soul = FMath::Max(0.f, Soul - Amount);
	LastOutcome = Reason;

	if (bLogJudge)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightFeel] soul -%.0f -> %.1f (wrong button)"), Amount, Soul);
	}

	OnDebugSoulChanged.Broadcast(Soul, LastOutcome);
}

// add by K2 (R1)
void UNightFeelStubComponent::PruneBufferedInputs()
{
	if (BufferedInputs.Num() == 0)
	{
		return;
	}
	const float Now = GetWorldTimeSeconds();
	const float MaxAge = FMath::Max(0.f, EarlyAcceptMs) * 0.001f;
	const int32 Removed = BufferedInputs.RemoveAll([Now, MaxAge](const FNightFeelBufferedInput& Item)
	{
		return (Now - Item.WorldTime) > MaxAge;
	});
	if (Removed > 0 && bLogJudge)
	{
		// 太早的空按：不罚血、不断连，只是消失
		UE_LOG(LogTemp, Log, TEXT("[NightFeel] dropped %d stale early input(s) (older than %.0fms)"),
			Removed, EarlyAcceptMs);
	}
}

// add by K2 (R1)
bool UNightFeelStubComponent::TryConsumeBufferedInput()
{
	if (!bHasActiveRequest || BufferedInputs.Num() == 0)
	{
		return false;
	}
	const float Now = GetWorldTimeSeconds();

	const FNightFeelBufferedInput Pending = BufferedInputs[0];
	BufferedInputs.RemoveAt(0);
	if (bLogJudge)
	{
		UE_LOG(LogTemp, Log, TEXT("[NightFeel] consume buffered input (early by %.0fms)"),
			(Now - Pending.WorldTime) * 1000.f);
	}
	JudgeInput(Pending.Input);
	return true;
}

// add by K2 (R1)
void UNightFeelStubComponent::RequestCatchUp()
{
	if (CatchUpPlayRate <= 1.f)
	{
		return;
	}
	if (ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(GetOwner()))
	{
		CoursePawn->ApplyAdvanceCatchUp(CatchUpPlayRate, FMath::Max(0.f, MaxCatchUpCompressMs) * 0.001f);
	}
}

// add by K2 (R1)
void UNightFeelStubComponent::ShowWindowDebug(ENightNodeKind Kind, float WindowSeconds) const
{
	const bool bAttack = (Kind == ENightNodeKind::Enemy);
	const FString Msg = FString::Printf(
		TEXT("WINDOW: %s  %.0fms  (idx=%d)"),
		GetInputHintText(bAttack),
		WindowSeconds * 1000.f,
		ActiveRequest.NodeIndex);
	PushHudLine(9911, 999.f, bAttack ? FColor::Red : FColor::Cyan, Msg);
}

// add by K2 (R1)
const TCHAR* UNightFeelStubComponent::GetInputHintText(bool bAttack) const
{
	const bool bSwapped = (ControlScheme == ENightControlScheme::Swapped);
	if (bAttack)
	{
		return bSwapped ? TEXT("ATTACK (Q)") : TEXT("ATTACK (E / LMB)");
	}
	return bSwapped ? TEXT("JUMP (E)") : TEXT("JUMP (Q)");
}

// add by K2 (R1)
void UNightFeelStubComponent::PushHudLine(int32 Key, float Duration, const FColor& Color, const FString& Msg) const
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(Key, Duration, Color, Msg);
	}
	if (bLogHudLines && !Msg.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("[NightFeel][HUD] %s"), *Msg);
	}
}

float UNightFeelStubComponent::GetSoul_Implementation() const
{
	return Soul;
}

void UNightFeelStubComponent::ApplySoulPenalty_Implementation(float Amount, ENightJudgeOutcome Reason)
{
	Soul = FMath::Max(0.f, Soul - Amount);
	LastOutcome = Reason;
	OnDebugSoulChanged.Broadcast(Soul, LastOutcome);
}

void UNightFeelStubComponent::PlaySuccessFeedback_Implementation(ENightNodeKind Kind)
{
	const bool bAttack = (Kind == ENightNodeKind::Enemy);
	PushHudLine(9912, 1.2f, FColor::Green, bAttack ? TEXT("HIT OK") : TEXT("JUMP OK"));

	// add by K2 (R1): 判定成功即起表现时钟，动作与随后的石间位移同时开始
	if (ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(GetOwner()))
	{
		CoursePawn->PlayHeroAction(bAttack);
	}
}

void UNightFeelStubComponent::PlayFailFeedback_Implementation(ENightJudgeOutcome Outcome, ENightNodeKind Kind)
{
	(void)Kind;
	const FString Msg = (Outcome == ENightJudgeOutcome::WrongButton)
		? TEXT("WRONG BUTTON")
		: TEXT("MISS");
	PushHudLine(9912, 1.2f, FColor::Orange, Msg);

	// add by K2 (R1): TA fail shake sits on miss / wrong-button, not on takeoff/land kicks.
	if (ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(GetOwner()))
	{
		CoursePawn->PlayFailCameraShake();
	}
}

void UNightFeelStubComponent::SetControlScheme_Implementation(ENightControlScheme Scheme)
{
	ControlScheme = Scheme;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			9913,
			2.0f,
			FColor::Magenta,
			ControlScheme == ENightControlScheme::Swapped
				? TEXT("KEYS SWAPPED: Q=ATTACK  E=JUMP")
				: TEXT("KEYS NORMAL: Q=JUMP  E=ATTACK"));
	}
}

ENightControlScheme UNightFeelStubComponent::GetControlScheme_Implementation() const
{
	return ControlScheme;
}

void UNightFeelStubComponent::HandleJump(const FInputActionValue& Value)
{
	(void)Value;
	// BlueprintNativeEvent interface: never call TryResolveInput() directly.
	TryResolveInput_Implementation(ENightFeelInput::Jump);
}

void UNightFeelStubComponent::HandleAttack(const FInputActionValue& Value)
{
	(void)Value;
	TryResolveInput_Implementation(ENightFeelInput::Attack);
}
#pragma endregion K2 moonyfli
