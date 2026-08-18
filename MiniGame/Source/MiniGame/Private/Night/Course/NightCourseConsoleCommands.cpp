#include "Night/Course/NightCourseHost.h"
#include "Night/Course/NightCourseDirector.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

#pragma region K2 moonyfli
static FAutoConsoleCommandWithWorld GNightCourseDumpCmd(
	TEXT("Night.Course.Dump"),
	TEXT("Dump G1 course host state"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			return;
		}
		TArray<AActor*> Hosts;
		UGameplayStatics::GetAllActorsOfClass(World, ANightCourseHost::StaticClass(), Hosts);
		for (AActor* Actor : Hosts)
		{
			if (ANightCourseHost* Host = Cast<ANightCourseHost>(Actor))
			{
				Host->DebugDumpState();
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GNightCourseFinishCmd(
	TEXT("Night.Course.Finish"),
	TEXT("Night.Course.Finish [0|1] force finish current course"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const bool bSuccess = Args.Num() == 0 || Args[0] != TEXT("0");
		TArray<AActor*> Hosts;
		UGameplayStatics::GetAllActorsOfClass(World, ANightCourseHost::StaticClass(), Hosts);
		for (AActor* Actor : Hosts)
		{
			if (ANightCourseHost* Host = Cast<ANightCourseHost>(Actor))
			{
				if (Host->Director)
				{
					Host->Director->DebugForceFinish(bSuccess);
				}
			}
		}
	}));

static FAutoConsoleCommandWithWorld GNightCourseSkipCmd(
	TEXT("Night.Course.SkipToExit"),
	TEXT("Skip remaining nodes and enter exit buffer"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		TArray<AActor*> Hosts;
		UGameplayStatics::GetAllActorsOfClass(World, ANightCourseHost::StaticClass(), Hosts);
		for (AActor* Actor : Hosts)
		{
			if (ANightCourseHost* Host = Cast<ANightCourseHost>(Actor))
			{
				if (Host->Director)
				{
					Host->Director->DebugSkipToExit();
				}
			}
		}
	}));
#pragma endregion K2 moonyfli
