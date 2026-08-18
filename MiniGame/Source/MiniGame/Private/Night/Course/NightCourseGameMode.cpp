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
		return;
	}

	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		if (USkyLightComponent* SkyComponent = It->GetLightComponent())
		{
			SkyComponent->bRealTimeCapture = true;
			SkyComponent->RecaptureSky();
		}
	}

	UClass* ClassToSpawn = HostClass ? HostClass.Get() : ANightCourseHost::StaticClass();
	ANightCourseHost* Host = World->SpawnActorDeferred<ANightCourseHost>(
		ClassToSpawn,
		FTransform::Identity,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!Host)
	{
		return;
	}

	Host->Config = CourseConfig;
	if (!Host->Config)
	{
		Host->Config = NewObject<UNightG1CourseConfig>(Host, TEXT("RuntimeG1Config"));
	}
	Host->Bootstrap.LevelId = ENightLevelId::T0;
	Host->Bootstrap.Seed = 1001;
	Host->bAutoStart = true;
	Host->FinishSpawning(FTransform::Identity);
	SpawnedHost = Host;
}
#pragma endregion K2 moonyfli
