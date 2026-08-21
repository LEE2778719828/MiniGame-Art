#include "Night/Course/NightCourseGameMode.h"
#include "Night/Course/NightCourseHost.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightCourseHUD.h"
#include "Night/Course/NightG1CourseConfig.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "EngineUtils.h"

#pragma region K2 moonyfli
ANightCourseGameMode::ANightCourseGameMode()
{
	DefaultPawnClass = ANightCoursePawn::StaticClass();
	HUDClass = ANightCourseHUD::StaticClass();
	HostClass = ANightCourseHost::StaticClass();
}
void ANightCourseGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=GameMode] BeginPlay aborted: World is null."));
		return;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=GameMode] BeginPlay map='%s' gameMode='%s' CourseConfig='%s' DefaultPawn='%s' HostClass='%s'."),
		*World->GetMapName(),
		*GetNameSafe(this),
		CourseConfig ? *CourseConfig->GetPathName() : TEXT("<null>"),
		*GetNameSafe(DefaultPawnClass),
		*GetNameSafe(HostClass));

	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		if (USkyLightComponent* SkyComponent = It->GetLightComponent())
		{
			SkyComponent->bRealTimeCapture = true;
			SkyComponent->RecaptureSky();
		}
	}

	for (TActorIterator<ANightCourseHost> It(World); It; ++It)
	{
		ANightCourseHost* ExistingHost = *It;
		if (ExistingHost)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[NightCourse][Stage=GameMode] Found existing Host='%s' serializedConfig='%s'."),
				*GetNameSafe(ExistingHost),
				ExistingHost->Config ? *ExistingHost->Config->GetPathName() : TEXT("<null>"));
			// The level host may contain a stale serialized Config from an
			// earlier preview. PIE must use the GameMode's current DataAsset.
			if (CourseConfig)
			{
				ExistingHost->Config = CourseConfig;
				UE_LOG(
					LogTemp,
					Display,
					TEXT("[NightCourse][Stage=GameMode] Applied GameMode CourseConfig='%s' to existing Host='%s'."),
					*CourseConfig->GetPathName(),
					*GetNameSafe(ExistingHost));
			}
			else
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[NightCourse][Stage=GameMode] CourseConfig is null; existing Host keeps serialized Config. Runtime may use stale or empty data."));
			}
			SpawnedHost = ExistingHost;
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[NightCourse][Stage=GameMode] Using existing Host='%s' finalConfig='%s'."),
				*GetNameSafe(SpawnedHost),
				SpawnedHost->Config ? *SpawnedHost->Config->GetPathName() : TEXT("<null>"));
			return;
		}
	}

	UClass* ClassToSpawn = HostClass ? HostClass.Get() : ANightCourseHost::StaticClass();
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=GameMode] No level Host found; spawning HostClass='%s' with CourseConfig='%s'."),
		*GetNameSafe(ClassToSpawn),
		CourseConfig ? *CourseConfig->GetPathName() : TEXT("<null>"));
	ANightCourseHost* Host = World->SpawnActorDeferred<ANightCourseHost>(
		ClassToSpawn,
		FTransform::Identity,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Host)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=GameMode] Failed to spawn HostClass='%s'."),
			*GetNameSafe(ClassToSpawn));
		return;
	}

	Host->Config = CourseConfig;
	if (!Host->Config)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[NightCourse][Stage=GameMode] Spawned Host='%s' without CourseConfig; transient fallback will not contain canonical Rule/Atom references."),
			*GetNameSafe(Host));
		Host->Config = NewObject<UNightG1CourseConfig>(Host, TEXT("RuntimeG1Config"));
	}
	Host->Bootstrap.LevelId = ENightLevelId::T0;
	Host->Bootstrap.Seed = 1001;
	Host->bAutoStart = true;
	Host->FinishSpawning(FTransform::Identity);
	SpawnedHost = Host;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[NightCourse][Stage=GameMode] Spawned Host='%s' finalConfig='%s' autoStart=%d seed=%d."),
		*GetNameSafe(Host),
		Host->Config ? *Host->Config->GetPathName() : TEXT("<null>"),
		Host->bAutoStart ? 1 : 0,
		Host->Bootstrap.Seed);
}
#pragma endregion K2 moonyfli
