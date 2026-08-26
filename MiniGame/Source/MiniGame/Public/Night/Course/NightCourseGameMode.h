#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NightCourseGameMode.generated.h"

class UNightG1CourseConfig;
class ANightCourseHost;
class AGameModeBase;
class UWorld;

#pragma region K2 moonyfli
/** G1 PIE entry: default pawn = NightCoursePawn, spawns CourseHost. */
UCLASS(Blueprintable, Config = Game)
class MINIGAME_API ANightCourseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANightCourseGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	TObjectPtr<UNightG1CourseConfig> CourseConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	TSubclassOf<ANightCourseHost> HostClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float MainHUDScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD", meta = (ClampMin = "0.0"))
	float MainHUDLeftMargin = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|HUD", meta = (ClampMin = "0.0"))
	float MainHUDTopMargin = 24.f;

	/** Fallback transition switch used when the Host has no destination override. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Night|Flow")
	bool bTravelToDayOnSuccess = true;

	/** Fallback Day level; normally configure the placed Host instead. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Night|Flow")
	TSoftObjectPtr<UWorld> SuccessDayLevel;

	/** Fallback Day GameMode; normally configure the placed Host instead. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Night|Flow")
	TSoftClassPtr<AGameModeBase> SuccessDayGameMode;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	TObjectPtr<ANightCourseHost> SpawnedHost;

protected:
	virtual void BeginPlay() override;
};
#pragma endregion K2 moonyfli
