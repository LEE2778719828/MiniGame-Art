#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Night/Course/NightCourseTypes.h"
#include "NightCourseRuleData.generated.h"

class UNightCourseAtomRouteData;

#pragma region K2 moonyfli
USTRUCT(BlueprintType)
struct MINIGAME_API FNightRuleAtomEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Rule")
	FString AtomKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Rule")
	TArray<ENightNodeKind> Actions;

	/** Weighted template probability when the queue is expanded to its target count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Rule", meta = (ClampMin = "1"))
	int32 Weight = 1;
};

USTRUCT(BlueprintType)
struct MINIGAME_API FNightRuleAtomQueue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Rule")
	TArray<FNightRuleAtomEntry> Atoms;

	/**
	 * Target number of generated Atoms for this branch.
	 * Zero keeps legacy behavior and uses Atoms.Num().
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Rule", meta = (ClampMin = "0"))
	int32 TargetAtomCount = 0;
};

/**
 * Planner-owned course rule. It deliberately contains only AtomKey and
 * action templates plus generation counts/weights; meshes, transforms,
 * landing positions and visual classes live in the Atom BP/library.
 */
UCLASS(BlueprintType)
class MINIGAME_API UNightCourseRuleData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule")
	bool bEnabled = true;

	/** Empty AtomKey values select deterministically from the configured Atom library. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule")
	bool bAutoSelectAtomKeys = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule")
	int32 Seed = 1001;

	/**
	 * Target number of generated base Atoms.
	 * Zero keeps legacy behavior and uses BaseRoute.Num().
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule|Queues", meta = (ClampMin = "0"))
	int32 BaseAtomCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule|Queues")
	TArray<FNightRuleAtomEntry> BaseRoute;

	/** Planner-owned branch queues. The Atom Library still owns only key -> BP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule|Queues")
	TMap<ENightRouteId, FNightRuleAtomQueue> BranchRoutes;

	/** Number of generated base atoms before the fork. INDEX_NONE means use the full base target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule|Queues")
	int32 ForkAfterBaseAtomIndex = INDEX_NONE;

	/** Optional text buffer used by the CallInEditor import/export helpers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Rule|JSON", meta = (MultiLine = true))
	FString EditorJson;

	UFUNCTION(BlueprintCallable, Category = "Night|Rule")
	bool ValidateRule(FString& OutError) const;

	UFUNCTION(BlueprintCallable, Category = "Night|Rule")
	bool ValidateRuleAgainstLibrary(
		const UNightCourseAtomRouteData* AtomLibrary,
		FString& OutError) const;

	UFUNCTION(BlueprintPure, Category = "Night|Rule")
	bool HasBranchRoute(ENightRouteId RouteId) const;

	UFUNCTION(BlueprintPure, Category = "Night|Rule")
	/** Returns the generated base count, not the number of templates. */
	int32 GetBaseRouteLength() const;

	UFUNCTION(BlueprintCallable, Category = "Night|Rule|JSON")
	bool ImportJson(const FString& JsonText, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Night|Rule|JSON")
	bool ExportJson(FString& OutJson, FString& OutError) const;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Night|Rule|JSON")
	void ImportEditorJson();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Night|Rule|JSON")
	void ExportEditorJson();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Night|Rule|Editor")
	void MarkPackageDirtyForEditor();
};
#pragma endregion K2 moonyfli
