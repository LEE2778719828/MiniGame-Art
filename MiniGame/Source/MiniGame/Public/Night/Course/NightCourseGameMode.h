#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NightCourseGameMode.generated.h"

class UNightG1CourseConfig;
class ANightCourseHost;

#pragma region K2 moonyfli
/** G1 PIE entry: default pawn = NightCoursePawn, spawns CourseHost. */
UCLASS(Blueprintable)
class MINIGAME_API ANightCourseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANightCourseGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	TObjectPtr<UNightG1CourseConfig> CourseConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	TSubclassOf<ANightCourseHost> HostClass;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course")
	TObjectPtr<ANightCourseHost> SpawnedHost;

protected:
	virtual void BeginPlay() override;
};
#pragma endregion K2 moonyfli
