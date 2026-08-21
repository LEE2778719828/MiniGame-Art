#include "Night/Course/NightCourseHost.h"
#include "Night/Course/NightCourseDirector.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "Night/Course/NightCourseAtomActor.h"
#include "Night/Course/NightCourseAtomRouteData.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/SoftObjectPath.h"

#pragma region K2 moonyfli
static FAutoConsoleCommandWithWorld GNightCourseDumpCmd(
	TEXT("Night.Course.Dump"),
	TEXT("Dump G1 course host state"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("[NightCourse][Stage=ConsoleDump] No World."));
			return;
		}
		TArray<AActor*> Hosts;
		UGameplayStatics::GetAllActorsOfClass(World, ANightCourseHost::StaticClass(), Hosts);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=ConsoleDump] map='%s' hosts=%d."),
			*World->GetMapName(),
			Hosts.Num());
		for (AActor* Actor : Hosts)
		{
			if (ANightCourseHost* Host = Cast<ANightCourseHost>(Actor))
			{
				Host->DebugDumpState();
			}
		}
	}));

static FAutoConsoleCommandWithWorld GNightCourseRebuildPreviewCmd(
	TEXT("Night.Course.RebuildPreview"),
	TEXT("Rebuild all NightCourseHost editor previews"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("[NightCourse][Stage=ConsolePreview] No World."));
			return;
		}
		TArray<AActor*> Hosts;
		UGameplayStatics::GetAllActorsOfClass(World, ANightCourseHost::StaticClass(), Hosts);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse][Stage=ConsolePreview] map='%s' hosts=%d."),
			*World->GetMapName(),
			Hosts.Num());
		for (AActor* Actor : Hosts)
		{
			if (ANightCourseHost* Host = Cast<ANightCourseHost>(Actor))
			{
				Host->RebuildEditorPreview();
			}
		}
	}));

static FAutoConsoleCommandWithWorld GNightCourseValidateCmd(
	TEXT("Night.Course.Validate"),
	TEXT("Validate NightCourse Config, Atom queues and RouteRules"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("[NightCourse][Stage=ConsoleValidate] No World."));
			return;
		}
		TArray<AActor*> Hosts;
		UGameplayStatics::GetAllActorsOfClass(World, ANightCourseHost::StaticClass(), Hosts);
		if (Hosts.Num() == 0)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse][Stage=ConsoleValidate] No NightCourseHost found in map='%s'."),
				*World->GetMapName());
			return;
		}
		for (AActor* Actor : Hosts)
		{
			if (ANightCourseHost* Host = Cast<ANightCourseHost>(Actor))
			{
				FString Error;
				const bool bValid = Host->Director
					&& Host->Director->ValidateConfiguration(Error);
				if (bValid)
				{
					UE_LOG(
						LogTemp,
						Display,
						TEXT("[NightCourse][Stage=ConsoleValidate] Host='%s' Config='%s' Validate OK."),
						*GetNameSafe(Host),
						Host->Config ? *Host->Config->GetPathName() : TEXT("<null>"));
				}
				else
				{
					if (!Host->Director)
					{
						Error = TEXT("Host has no Director component.");
					}
					else if (Error.IsEmpty())
					{
						Error = TEXT("Validation failed without an error string.");
					}
					UE_LOG(
						LogTemp,
						Error,
						TEXT("[NightCourse][Stage=ConsoleValidate] Host='%s' Config='%s' Validate FAILED: %s"),
						*GetNameSafe(Host),
						Host->Config ? *Host->Config->GetPathName() : TEXT("<null>"),
						*Error);
				}
			}
		}
	}));

static FAutoConsoleCommandWithWorld GNightCourseChooseLeftCmd(
	TEXT("Night.Course.ChooseLeft"),
	TEXT("Choose the left route while a fork is active"),
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
					Host->Director->ChooseForkLeft();
				}
			}
		}
	}));

static FAutoConsoleCommandWithWorld GNightCourseChooseRightCmd(
	TEXT("Night.Course.ChooseRight"),
	TEXT("Choose the right route while a fork is active"),
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
					Host->Director->ChooseForkRight();
				}
			}
		}
	}));

static FAutoConsoleCommandWithWorld GNightCourseSkipForkCmd(
	TEXT("Night.Course.SkipFork"),
	TEXT("Resolve the active fork using its configured timeout-left policy"),
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
					Host->Director->SkipFork();
				}
			}
		}
	}));

static FAutoConsoleCommandWithWorld GNightCourseForceKeySwapCmd(
	TEXT("Night.Course.ForceKeySwap"),
	TEXT("Force the next configured key-swap cue"),
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
					Host->Director->ForceKeySwap();
				}
			}
		}
	}));

static FAutoConsoleCommandWithWorld GNightCourseResetCmd(
	TEXT("Night.Course.Reset"),
	TEXT("Reset NightCourse runtime state and destroy spawned course actors"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		TArray<AActor*> Hosts;
		UGameplayStatics::GetAllActorsOfClass(World, ANightCourseHost::StaticClass(), Hosts);
		for (AActor* Actor : Hosts)
		{
			if (ANightCourseHost* Host = Cast<ANightCourseHost>(Actor))
			{
				Host->ResetCourse();
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

static FAutoConsoleCommandWithWorldAndArgs GNightCourseRegisterAtomCmd(
	TEXT("Night.Atom.Register"),
	TEXT("Night.Atom.Register <libraryDataAsset> <key> <atomGeneratedClassPath>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
	{
		if (Args.Num() < 3)
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse] Usage: Night.Atom.Register /Game/.../Library Key /Game/.../BP.BP_C"));
			return;
		}

		UNightCourseAtomRouteData* Library =
			LoadObject<UNightCourseAtomRouteData>(nullptr, *Args[0]);
		if (!Library)
		{
			UE_LOG(LogTemp, Error, TEXT("[NightCourse] Atom Library not found: %s"), *Args[0]);
			return;
		}

		const FSoftObjectPath ClassPath(Args[2]);
		UClass* AtomClass = LoadObject<UClass>(nullptr, *ClassPath.ToString());
		if (!AtomClass || !AtomClass->IsChildOf(ANightCourseAtomActor::StaticClass()))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[NightCourse] Atom class is invalid: %s"),
				*Args[2]);
			return;
		}

		Library->Modify();
		Library->AtomMap.Add(
			Args[1],
			TSoftClassPtr<ANightCourseAtomActor>(ClassPath));
		Library->MarkPackageDirtyForEditor();
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[NightCourse] Registered Atom '%s' -> %s in %s."),
			*Args[1],
			*Args[2],
			*Args[0]);
	}));

static FAutoConsoleCommandWithWorldAndArgs GNightCourseSnapAtomLandingCmd(
	TEXT("Night.Atom.SnapLanding"),
	TEXT("Night.Atom.SnapLanding <atomBlueprintPath>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Error, TEXT("[NightCourse] Usage: Night.Atom.SnapLanding /Game/.../BP_Atom"));
			return;
		}

		UClass* AtomClass = LoadObject<UClass>(nullptr, *(Args[0] + TEXT(".") + FPackageName::GetShortName(Args[0]) + TEXT("_C")));
		if (!AtomClass || !AtomClass->IsChildOf(ANightCourseAtomActor::StaticClass()))
		{
			UE_LOG(LogTemp, Error, TEXT("[NightCourse] Atom BP not found: %s"), *Args[0]);
			return;
		}

		if (ANightCourseAtomActor* AtomCDO = AtomClass->GetDefaultObject<ANightCourseAtomActor>())
		{
			AtomCDO->SnapFirstLastLandingToAnchors();
			UE_LOG(LogTemp, Display, TEXT("[NightCourse] Snapped landing points in Atom BP '%s'."), *Args[0]);
		}
	}));
#pragma endregion K2 moonyfli
