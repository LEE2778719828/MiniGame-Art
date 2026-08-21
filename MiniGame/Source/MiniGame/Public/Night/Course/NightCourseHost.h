#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Night/Course/NightCourseInterface.h"
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
class UDirectionalLightComponent;
class UBoxComponent;
class AStaticMeshActor;
class AActor;

#pragma region K2 moonyfli
/**
 * Place one in level (or spawn from GameMode). Owns Director, wires Feel, auto-starts G1.
 */
UCLASS(Blueprintable)
class MINIGAME_API ANightCourseHost : public AActor, public INightCourse
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

	/** Use USChefGameInstance's Night → Day inventory and retry flow when present. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Flow")
	bool bUseChefDayFlow = true;

	/** Automatically restart a run after a gameplay failure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Flow")
	bool bAutoRetryOnFailure = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Flow", meta = (ClampMin = "0.05"))
	float AutoRetryDelaySeconds = 0.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Course|Layout")
	TObjectPtr<UBoxComponent> LayoutBounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Layout")
	bool bEnforceLayoutBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Layout")
	FVector LayoutBoundsExtent = FVector(10000.f, 10000.f, 3000.f);

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Debug")
	FNightResult LastResult;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Result")
	bool bHasResult = false;

	UPROPERTY(BlueprintReadOnly, Category = "Night|Course|Debug")
	FString LastFailureReason;

	UPROPERTY(BlueprintAssignable, Category = "Night|Course|Result")
	FOnNightCourseFinished OnNightFinished;

	UPROPERTY(BlueprintAssignable, Category = "Night|Course|Result")
	FOnNightCourseFinished OnNightSucceeded;

	UPROPERTY(BlueprintAssignable, Category = "Night|Course|Result")
	FOnNightCourseFinished OnNightFailed;

	UPROPERTY(BlueprintAssignable, Category = "Night|Course|Debug")
	FOnNightCourseDebugMessage OnDebugMessage;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Presentation")
	TObjectPtr<UExponentialHeightFogComponent> NightFog;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Editor Preview")
	TObjectPtr<UInstancedStaticMeshComponent> PreviewBridgeA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Editor Preview")
	TObjectPtr<UInstancedStaticMeshComponent> PreviewBridgeB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Editor Preview")
	TObjectPtr<UInstancedStaticMeshComponent> PreviewFoeM01;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Editor Preview")
	TObjectPtr<UInstancedStaticMeshComponent> PreviewFoeM02;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Editor Preview")
	TObjectPtr<UInstancedStaticMeshComponent> PreviewFoeM03;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Editor Preview")
	TObjectPtr<UInstancedStaticMeshComponent> PreviewFoeM04;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Editor Preview")
	TObjectPtr<UInstancedStaticMeshComponent> PreviewFoeM05;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Editor Preview")
	TObjectPtr<UDirectionalLightComponent> PreviewKeyLight;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Night|Editor Preview")
	TArray<TObjectPtr<AActor>> EditorPreviewMeshActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Lighting")
	float RuntimeKeyLightIntensity = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Lighting")
	FLinearColor RuntimeKeyLightColor = FLinearColor(0.72f, 0.82f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Lighting")
	float RuntimeFogDensity = 0.003f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Editor Preview|Material")
	TSoftObjectPtr<UMaterialInterface> EditorPreviewMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Editor Preview|Material")
	FLinearColor EditorPreviewBridgeColorA = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Editor Preview|Material")
	FLinearColor EditorPreviewBridgeColorB = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Editor Preview|Material")
	FLinearColor EditorPreviewFoeColorM01 = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Editor Preview|Material")
	FLinearColor EditorPreviewFoeColorM02 = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Editor Preview|Material")
	FLinearColor EditorPreviewFoeColorM03 = FLinearColor::White;

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	void StartCourse();

	UFUNCTION(BlueprintCallable, Category = "Night|Course")
	bool TryStartCourse(FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Night|Course|Debug")
	void ResetCourse();

	UFUNCTION(BlueprintPure, Category = "Night|Course|Result")
	bool HasCourseResult() const { return bHasResult; }

	UFUNCTION(BlueprintPure, Category = "Night|Course|Result")
	FNightResult GetCourseResult() const { return LastResult; }

	UFUNCTION(BlueprintPure, Category = "Night|Course|Debug")
	FString GetLastFailureReason() const { return LastFailureReason; }

	UFUNCTION(CallInEditor, Category = "Night|Editor Preview")
	void RebuildEditorPreview();

	UFUNCTION(BlueprintCallable, Category = "Night|Course|Debug")
	void DebugDumpState() const;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual bool StartNight_Implementation(
		const FNightBootstrap& InBootstrap,
		FString& OutError) override;
	virtual void ResetNight_Implementation() override;
	virtual bool HasNightResult_Implementation() const override;
	virtual FNightResult GetNightResult_Implementation() const override;

	UFUNCTION()
	void HandleFinished(const FNightResult& Result);

	UFUNCTION()
	void HandleDirectorDebugMessage(const FString& Message, bool bIsError);

	UFUNCTION()
	void HandleFeelResolved(int32 NodeIndex, ENightJudgeOutcome Outcome);

	void WireFeelFromPlayer();
	void BuildPlayableStage();
	void ClearCourseResult();
	void EmitDebugMessage(const FString& Message, bool bIsError);
	void PrepareChefNightFlow();
	void RetryAfterFailure();
	void TravelToDay();

	UPROPERTY()
	TObjectPtr<UStaticMesh> StageCubeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> StageMaterial;

	FTimerHandle AutoStartTimer;
	FTimerHandle RetryTimer;
};
#pragma endregion K2 moonyfli
