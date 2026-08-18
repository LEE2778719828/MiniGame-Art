#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Night/Shared/NightSharedTypes.h"
#include "Night/Course/NightCourseTypes.h"
#include "NightCourseHost.generated.h"

class UNightCourseDirector;
class UNightG1CourseConfig;
class ANightCoursePawn;
class UStaticMesh;
class UMaterialInterface;
class UExponentialHeightFogComponent;
class UInstancedStaticMeshComponent;

#pragma region K2 moonyfli
/**
 * Place one in level (or spawn from GameMode). Owns Director, wires Feel, auto-starts G1.
 */
UCLASS(Blueprintable)
class MINIGAME_API ANightCourseHost : public AActor
{
	GENERATED_BODY()

public:
	ANightCourseHost();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Course")
	TObjectPtr<UNightCourseDirector> Director;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	TObjectPtr<UNightG1CourseConfig> Config;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course")
	FNightBootstrap Bootstrap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Debug")
	bool bAutoStart = true;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Debug")
	FNightResult LastResult;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Presentation")
	TObjectPtr<UExponentialHeightFogComponent> NightFog;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Editor Preview")
	TObjectPtr<UInstancedStaticMeshComponent> PreviewBridgeA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Editor Preview")
	TObjectPtr<UInstancedStaticMeshComponent> PreviewBridgeB;

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void StartCourse();

	UFUNCTION(CallInEditor, Category = "Night|Editor Preview")
	void RebuildEditorPreview();

	UFUNCTION(BlueprintCallable, Category = "Night|Course|Debug")
	void DebugDumpState() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION()
	void HandleFinished(const FNightResult& Result);

	UFUNCTION()
	void HandleFeelResolved(int32 NodeIndex, ENightJudgeOutcome Outcome);

	void WireFeelFromPlayer();
	void BuildPlayableStage();

	UPROPERTY()
	TObjectPtr<UStaticMesh> StageCubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> StageMaterial;

	FTimerHandle AutoStartTimer;
};
#pragma endregion K2 moonyfli
