#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightFeelBridge.h"
#include "Night/Course/NightCoursePawn.h"
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
		UE_LOG(LogTemp, Warning, TEXT("  按错扣魂(hazard/enemy)=%.0f/%.0f invuln=%.0f | 呼吸扣血 on=%d rate=%.2f/s"),
			Feel->SoulPenaltyHazard, Feel->SoulPenaltyEnemy, Feel->HitInvulnMs,
			Feel->bEnableBreathDecay ? 1 : 0, Feel->BreathDecayPerSecond);
		UE_LOG(LogTemp, Warning, TEXT("  logHud=%d"), Feel->bLogHudLines ? 1 : 0);
	}));

static FAutoConsoleCommandWithWorldAndArgs GNightFeelSetCmd(
	TEXT("Night.Feel.Set"),
	TEXT("Night.Feel.Set <Param> <Value> — live-tune feel params. Params: Jump, Attack, Tutorial, Early, CatchUp, MaxCompress, Hazard, Enemy, Invuln, Soul, Breath, BreathOn, LogHud"),
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
#pragma endregion K2 (R1)
