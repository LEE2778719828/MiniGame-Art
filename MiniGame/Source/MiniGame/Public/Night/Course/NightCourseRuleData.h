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
};

/**
 * Planner-owned course rule. It deliberately contains only AtomKey and
 * actions; meshes, transforms, landing positions and visual classes live in
 * the Atom BP/library.
 */
UCLASS(BlueprintType)
class MINIGAME_API UNightCourseRuleData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule")
	int32 Seed = 1001;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule")
	TArray<FNightRuleAtomEntry> Route;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Rule")
	ENightNodeKind TransitionAction = ENightNodeKind::Hazard;

	/** Optional text buffer used by the CallInEditor import/export helpers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Rule|JSON", meta = (MultiLine = true))
	FString EditorJson;

	UFUNCTION(BlueprintCallable, Category = "Night|Rule")
	bool ValidateRule(FString& OutError) const;

	UFUNCTION(BlueprintCallable, Category = "Night|Rule")
	bool ValidateRuleAgainstLibrary(
		const UNightCourseAtomRouteData* AtomLibrary,
		FString& OutError) const;

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
