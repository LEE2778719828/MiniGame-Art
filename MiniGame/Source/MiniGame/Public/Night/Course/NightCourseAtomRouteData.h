#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NightCourseAtomRouteData.generated.h"

class ANightCourseAtomActor;

#pragma region K2 moonyfli
/**
 * Artist-owned registry/library for authored course atoms.
 *
 * AtomMap is the reusable library (key -> Blueprint class). When a
 * CourseRuleData is enabled, the planner rule owns the queue and this asset
 * contributes the artist-authored Atom candidates and transition gap.
 */
UCLASS(BlueprintType)
class MINIGAME_API UNightCourseAtomRouteData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Atom Library")
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Atom Library")
	TMap<FString, TSoftClassPtr<ANightCourseAtomActor>> AtomMap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Atom Library")
	float TransitionJumpGapCm = 520.f;

	/** Returns stable, valid candidates whose authored landing-point count matches the action count. */
	void GetCompatibleAtomKeys(
		int32 RequiredActionCount,
		TArray<FString>& OutKeys,
		TArray<FString>* OutRejectionReasons = nullptr) const;

	UFUNCTION(BlueprintCallable, Category = "Night|Atom Route")
	bool ValidateRoute(FString& OutError) const;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Night|Atom Route|Editor")
	void MarkPackageDirtyForEditor();
};
#pragma endregion K2 moonyfli
