#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Night/Shared/NightSharedTypes.h"
#include "NightCourseInterface.generated.h"

/**
 * Public result/debug delegates shared by the NightCourse implementation and
 * its host. They intentionally carry gameplay state only; presentation is
 * owned by the caller.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnNightCourseFinished,
	const FNightResult&,
	Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnNightCourseDebugMessage,
	const FString&,
	Message,
	bool,
	bIsError);

UINTERFACE(BlueprintType)
class MINIGAME_API UNightCourse : public UInterface
{
	GENERATED_BODY()
};

/**
 * R2 night-course boundary used by S or a test harness.
 *
 * The result delegates live on ANightCourseHost because multicast delegates
 * are UObject state. This interface only defines the callable lifecycle and
 * result query surface.
 */
class MINIGAME_API INightCourse
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Course")
	bool StartNight(const FNightBootstrap& Bootstrap, FString& OutError);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Course")
	void ResetNight();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Course|Result")
	bool HasNightResult() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Course|Result")
	FNightResult GetNightResult() const;
};
