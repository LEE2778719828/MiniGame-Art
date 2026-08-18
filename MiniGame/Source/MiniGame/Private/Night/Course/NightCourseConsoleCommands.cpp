#include "Night/Course/NightCourseHost.h"
#include "Night/Course/NightCourseDirector.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

#pragma region K2 moonyfli
static ANightCourseHost* FindFirstCourseHost(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	TArray<AActor*> Hosts;
	UGameplayStatics::GetAllActorsOfClass(World, ANightCourseHost::StaticClass(), Hosts);
	for (AActor* Actor : Hosts)
	{
		if (ANightCourseHost* Host = Cast<ANightCourseHost>(Actor))
		{
			return Host;
		}
	}
	return nullptr;
}

static FAutoConsoleCommandWithWorld GNightCourseDumpCmd(
	TEXT("Night.Course.Dump"),
	TEXT("Dump G1/G2 course host state"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ANightCourseHost* Host = FindFirstCourseHost(World))
		{
			Host->DebugDumpState();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GNightCourseFinishCmd(
	TEXT("Night.Course.Finish"),
	TEXT("Night.Course.Finish [0|1] force finish current course"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const bool bSuccess = Args.Num() == 0 || Args[0] != TEXT("0");
		if (ANightCourseHost* Host = FindFirstCourseHost(World))
		{
			if (Host->Director)
			{
				Host->Director->DebugForceFinish(bSuccess);
			}
		}
	}));

static FAutoConsoleCommandWithWorld GNightCourseSkipCmd(
	TEXT("Night.Course.SkipToExit"),
	TEXT("Skip remaining nodes and enter exit buffer"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ANightCourseHost* Host = FindFirstCourseHost(World))
		{
			if (Host->Director)
			{
				Host->Director->DebugSkipToExit();
			}
		}
	}));

static FAutoConsoleCommandWithWorld GNightCourseChooseLeftCmd(
	TEXT("Night.Course.ChooseLeft"),
	TEXT("Choose left fork card during ForkChoice"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ANightCourseHost* Host = FindFirstCourseHost(World))
		{
			if (Host->Director)
			{
				Host->Director->ChooseForkLeft();
			}
		}
	}));

static FAutoConsoleCommandWithWorld GNightCourseChooseRightCmd(
	TEXT("Night.Course.ChooseRight"),
	TEXT("Choose right fork card during ForkChoice"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ANightCourseHost* Host = FindFirstCourseHost(World))
		{
			if (Host->Director)
			{
				Host->Director->ChooseForkRight();
			}
		}
	}));

static FAutoConsoleCommandWithWorld GNightCourseSkipForkCmd(
	TEXT("Night.Course.SkipFork"),
	TEXT("Skip fork by taking left/default route"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ANightCourseHost* Host = FindFirstCourseHost(World))
		{
			if (Host->Director)
			{
				Host->Director->DebugSkipFork();
			}
		}
	}));

static FAutoConsoleCommandWithWorld GNightCourseForceKeySwapCmd(
	TEXT("Night.Course.ForceKeySwap"),
	TEXT("Force a key-swap warning/safety on the branch segment"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ANightCourseHost* Host = FindFirstCourseHost(World))
		{
			if (Host->Director)
			{
				Host->Director->DebugForceKeySwap();
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GNightCourseImportParamsCmd(
	TEXT("Night.Course.ImportParams"),
	TEXT("Night.Course.ImportParams <jsonPath> then restart course with proc params"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("Usage: Night.Course.ImportParams <path.json>"));
			return;
		}
		if (ANightCourseHost* Host = FindFirstCourseHost(World))
		{
			if (Host->Director && Host->Director->ImportProcParamsFromJsonFile(Args[0]))
			{
				Host->ProcParamsAsset = Host->Director->ProcParamsAsset;
				if (Host->Director->IsRunning())
				{
					Host->Director->DebugForceFinish(false);
				}
				Host->StartCourse();
			}
		}
	}));
#pragma endregion K2 moonyfli
