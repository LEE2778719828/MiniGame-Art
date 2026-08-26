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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Atom Library", meta = (DisplayName = "启用Atom库", ToolTip = "开启后 DA_Rules 中的 AtomKey 才能从本库解析；正式运行请保持开启。"))
	bool bEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Atom Library", meta = (DisplayName = "Atom Blueprint映射", ToolTip = "填写 AtomKey 到 Atom Blueprint 的映射；Key 必须唯一，Blueprint 需要包含有效 LandingPoint。"))
	TMap<FString, TSoftClassPtr<ANightCourseAtomActor>> AtomMap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Atom Library", meta = (DisplayName = "Atom衔接跳跃间距", ToolTip = "相邻 Atom 之间的默认衔接距离，单位 cm；用于课程自动拼接。"))
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
