#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightFeelBridge.h"
#include "Night/Course/NightCoursePawn.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#pragma region K2 (R1)
namespace NightFeelConsole_Private
{
	static UNightFeelStubComponent* FindFeel(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		if (ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(UGameplayStatics::GetPlayerPawn(World, 0)))
		{
			return CoursePawn->FeelStub;
		}
		TArray<AActor*> Pawns;
		UGameplayStatics::GetAllActorsOfClass(World, ANightCoursePawn::StaticClass(), Pawns);
		for (AActor* Actor : Pawns)
		{
			if (ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(Actor))
			{
				if (CoursePawn->FeelStub)
				{
					return CoursePawn->FeelStub;
				}
			}
		}
		return nullptr;
	}

	static ANightCoursePawn* FindPawn(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		if (ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(UGameplayStatics::GetPlayerPawn(World, 0)))
		{
			return CoursePawn;
		}
		TArray<AActor*> Pawns;
		UGameplayStatics::GetAllActorsOfClass(World, ANightCoursePawn::StaticClass(), Pawns);
		return Pawns.Num() > 0 ? Cast<ANightCoursePawn>(Pawns[0]) : nullptr;
	}

	static const TCHAR* PhaseName(ENightFeelPhase Phase)
	{
		switch (Phase)
		{
		case ENightFeelPhase::WindowOpen: return TEXT("WindowOpen");
		case ENightFeelPhase::Breathing:  return TEXT("Breathing");
		default:                          return TEXT("Idle");
		}
	}
}

static FAutoConsoleCommandWithWorld GNightFeelStatusCmd(
	TEXT("Night.Feel.Status"),
	TEXT("Dump R1 feel state: phase, window, buffer, tuning params"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UNightFeelStubComponent* Feel = NightFeelConsole_Private::FindFeel(World);
		if (!Feel)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NightFeel] no FeelStub found (PIE running?)"));
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("[NightFeel] phase=%s soul=%.1f windowLeft=%.0fms"),
			NightFeelConsole_Private::PhaseName(Feel->Phase), Feel->Soul, Feel->WindowRemainingSeconds * 1000.f);
		UE_LOG(LogTemp, Warning, TEXT("  空档 jump=%.0f attack=%.0f tutorial=%d(%.0f/%.0f)"),
			Feel->JumpWindowMs, Feel->AttackWindowMs,
			Feel->bUseTutorialWindows ? 1 : 0, Feel->TutorialJumpWindowMs, Feel->TutorialAttackWindowMs);
		UE_LOG(LogTemp, Warning, TEXT("  early=%.0f catchUp=%.2f maxCompress=%.0f"),
			Feel->EarlyAcceptMs, Feel->CatchUpPlayRate, Feel->MaxCatchUpCompressMs);
		UE_LOG(LogTemp, Warning, TEXT("  空档等动画 on=%d 上一拍等了 %.0fms（裁定 R-007）"),
			Feel->bGraceWaitsForAnim ? 1 : 0, Feel->LastAnimTailSeconds * 1000.f);
		UE_LOG(LogTemp, Warning, TEXT("  按错扣魂(hazard/enemy)=%.0f/%.0f invuln=%.0f | 呼吸扣血 on=%d rate=%.2f/s"),
			Feel->SoulPenaltyHazard, Feel->SoulPenaltyEnemy, Feel->HitInvulnMs,
			Feel->bEnableBreathDecay ? 1 : 0, Feel->BreathDecayPerSecond);
		UE_LOG(LogTemp, Warning, TEXT("  logHud=%d"), Feel->bLogHudLines ? 1 : 0);
	}));

static FAutoConsoleCommandWithWorldAndArgs GNightFeelSetCmd(
	TEXT("Night.Feel.Set"),
	TEXT("Night.Feel.Set <Param> <Value> — live-tune feel params. Params: Jump, Attack, Tutorial, Early, CatchUp, MaxCompress, Hazard, Enemy, Invuln, Soul, Breath, BreathOn, LogHud, GraceWaitsAnim"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UNightFeelStubComponent* Feel = NightFeelConsole_Private::FindFeel(World);
		if (!Feel)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NightFeel] no FeelStub found (PIE running?)"));
			return;
		}
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NightFeel] usage: Night.Feel.Set <Param> <Value>"));
			return;
		}

		const FString& Key = Args[0];
		const float Value = FCString::Atof(*Args[1]);
		if      (Key.Equals(TEXT("Jump"), ESearchCase::IgnoreCase))        { Feel->JumpWindowMs = Value; }
		else if (Key.Equals(TEXT("Attack"), ESearchCase::IgnoreCase))      { Feel->AttackWindowMs = Value; }
		else if (Key.Equals(TEXT("Tutorial"), ESearchCase::IgnoreCase))    { Feel->bUseTutorialWindows = (Value != 0.f); }
		else if (Key.Equals(TEXT("Early"), ESearchCase::IgnoreCase))       { Feel->EarlyAcceptMs = Value; }
		else if (Key.Equals(TEXT("CatchUp"), ESearchCase::IgnoreCase))     { Feel->CatchUpPlayRate = Value; }
		else if (Key.Equals(TEXT("MaxCompress"), ESearchCase::IgnoreCase)) { Feel->MaxCatchUpCompressMs = Value; }
		else if (Key.Equals(TEXT("Hazard"), ESearchCase::IgnoreCase))      { Feel->SoulPenaltyHazard = Value; }
		else if (Key.Equals(TEXT("Enemy"), ESearchCase::IgnoreCase))       { Feel->SoulPenaltyEnemy = Value; }
		else if (Key.Equals(TEXT("Invuln"), ESearchCase::IgnoreCase))      { Feel->HitInvulnMs = Value; }
		else if (Key.Equals(TEXT("Soul"), ESearchCase::IgnoreCase))        { Feel->Soul = Value; }
		else if (Key.Equals(TEXT("Breath"), ESearchCase::IgnoreCase))      { Feel->BreathDecayPerSecond = Value; }
		else if (Key.Equals(TEXT("BreathOn"), ESearchCase::IgnoreCase))    { Feel->bEnableBreathDecay = (Value != 0.f); }
		else if (Key.Equals(TEXT("LogHud"), ESearchCase::IgnoreCase))      { Feel->bLogHudLines = (Value != 0.f); }
		else if (Key.Equals(TEXT("GraceWaitsAnim"), ESearchCase::IgnoreCase)) { Feel->bGraceWaitsForAnim = (Value != 0.f); }
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[NightFeel] unknown param '%s'"), *Key);
			return;
		}
		UE_LOG(LogTemp, Warning, TEXT("[NightFeel] set %s = %.2f"), *Key, Value);
	}));

static FAutoConsoleCommandWithWorldAndArgs GNightFeelPressCmd(
	TEXT("Night.Feel.Press"),
	TEXT("Night.Feel.Press <Jump|Attack> [Count] [IntervalMs] — inject input without touching the keyboard"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UNightFeelStubComponent* Feel = NightFeelConsole_Private::FindFeel(World);
		if (!Feel || !World)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NightFeel] no FeelStub found (PIE running?)"));
			return;
		}
		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NightFeel] usage: Night.Feel.Press <Jump|Attack> [Count] [IntervalMs]"));
			return;
		}

		const ENightFeelInput Input = Args[0].Equals(TEXT("Attack"), ESearchCase::IgnoreCase)
			? ENightFeelInput::Attack
			: ENightFeelInput::Jump;
		const int32 Count = (Args.Num() >= 2) ? FMath::Max(1, FCString::Atoi(*Args[1])) : 1;
		const float IntervalSeconds = (Args.Num() >= 3) ? FMath::Max(0.f, FCString::Atof(*Args[2])) * 0.001f : 0.f;

		INightFeelBridge::Execute_TryResolveInput(Feel, Input);
		if (IntervalSeconds <= KINDA_SMALL_NUMBER)
		{
			for (int32 Index = 1; Index < Count; ++Index)
			{
				INightFeelBridge::Execute_TryResolveInput(Feel, Input);
			}
			return;
		}

		const TWeakObjectPtr<UNightFeelStubComponent> WeakFeel(Feel);
		for (int32 Index = 1; Index < Count; ++Index)
		{
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(
				Handle,
				FTimerDelegate::CreateLambda([WeakFeel, Input]()
				{
					if (UNightFeelStubComponent* Target = WeakFeel.Get())
					{
						INightFeelBridge::Execute_TryResolveInput(Target, Input);
					}
				}),
				IntervalSeconds * Index,
				false);
		}
	}));

// add by K2 (R1)
// 常速斩击缺位期间，快版要拨到多慢才顺，只能按出来。放控制台省去每次改参数重编译。
static FAutoConsoleCommandWithWorld GNightAnimStatusCmd(
	TEXT("Night.Anim.Status"),
	TEXT("Dump hero animation clips, base play rates and the resulting durations"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		ANightCoursePawn* CoursePawn = NightFeelConsole_Private::FindPawn(World);
		if (!CoursePawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NightAnim] no course pawn found (PIE running?)"));
			return;
		}

		auto Describe = [](const TCHAR* Label, UAnimSequence* Clip, float Rate)
		{
			if (!Clip)
			{
				UE_LOG(LogTemp, Warning, TEXT("  %s: <none>"), Label);
				return;
			}
			const float RawMs = Clip->GetPlayLength() * 1000.f;
			const float SafeRate = FMath::Max(0.05f, Rate);
			UE_LOG(LogTemp, Warning, TEXT("  %s: %s  raw=%.0fms  rate=%.2f  ->  %.0fms"),
				Label, *Clip->GetName(), RawMs, SafeRate, RawMs / SafeRate);
		};

		UE_LOG(LogTemp, Warning, TEXT("[NightAnim] currently playing at rate %.2f (base x catch-up), %.0fms left"),
			CoursePawn->GetHeroAnimPlayRate(), CoursePawn->GetHeroActionRemainingSeconds() * 1000.f);
		Describe(TEXT("Jump  "), CoursePawn->JumpAnim, CoursePawn->JumpAnimRate);
		Describe(TEXT("Attack"), CoursePawn->AttackAnim, CoursePawn->AttackAnimRate);
		UE_LOG(LogTemp, Warning, TEXT("  动画驱动位移 on=%d  锚点 jump=%.0fms attack=%.0fms"),
			CoursePawn->bAnimDrivenAdvance ? 1 : 0, CoursePawn->JumpAnchorMs, CoursePawn->AttackAnchorMs);
	}));

// add by K2 (R1)
static FAutoConsoleCommandWithWorldAndArgs GNightAnimDriveCmd(
	TEXT("Night.Anim.Drive"),
	TEXT("Night.Anim.Drive <0|1> [JumpAnchorMs] [AttackAnchorMs] — 1 makes the stone-to-stone move last "
		 "as long as the animation's anchor (landing / contact) instead of using the configured AdvanceSpeed"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ANightCoursePawn* CoursePawn = NightFeelConsole_Private::FindPawn(World);
		if (!CoursePawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NightAnim] no course pawn found (PIE running?)"));
			return;
		}
		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NightAnim] usage: Night.Anim.Drive <0|1> [JumpAnchorMs] [AttackAnchorMs]"));
			return;
		}

		CoursePawn->bAnimDrivenAdvance = (FCString::Atof(*Args[0]) != 0.f);
		if (Args.Num() >= 2)
		{
			CoursePawn->JumpAnchorMs = FMath::Max(10.f, FCString::Atof(*Args[1]));
		}
		if (Args.Num() >= 3)
		{
			CoursePawn->AttackAnchorMs = FMath::Max(10.f, FCString::Atof(*Args[2]));
		}

		UE_LOG(LogTemp, Warning, TEXT("[NightAnim] 动画驱动位移 = %d  锚点 jump=%.0fms attack=%.0fms (下一次移动生效)"),
			CoursePawn->bAnimDrivenAdvance ? 1 : 0, CoursePawn->JumpAnchorMs, CoursePawn->AttackAnchorMs);
	}));

static FAutoConsoleCommandWithWorldAndArgs GNightAnimRateCmd(
	TEXT("Night.Anim.Rate"),
	TEXT("Night.Anim.Rate <Jump|Attack> <Rate> — set the base play rate (<1 slower, >1 faster). Takes effect on the next action."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ANightCoursePawn* CoursePawn = NightFeelConsole_Private::FindPawn(World);
		if (!CoursePawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NightAnim] no course pawn found (PIE running?)"));
			return;
		}
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NightAnim] usage: Night.Anim.Rate <Jump|Attack> <Rate>"));
			return;
		}

		const bool bAttack = Args[0].Equals(TEXT("Attack"), ESearchCase::IgnoreCase);
		const float Rate = FMath::Clamp(FCString::Atof(*Args[1]), 0.05f, 4.f);
		UAnimSequence* Clip = bAttack ? CoursePawn->AttackAnim : CoursePawn->JumpAnim;

		if (bAttack)
		{
			CoursePawn->AttackAnimRate = Rate;
		}
		else
		{
			CoursePawn->JumpAnimRate = Rate;
		}

		const float RawMs = Clip ? Clip->GetPlayLength() * 1000.f : 0.f;
		UE_LOG(LogTemp, Warning, TEXT("[NightAnim] %s rate = %.2f  (%.0fms -> %.0fms)"),
			bAttack ? TEXT("Attack") : TEXT("Jump"), Rate, RawMs, RawMs / Rate);
	}));
#pragma endregion K2 (R1)
