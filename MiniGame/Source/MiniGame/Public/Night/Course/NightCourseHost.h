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
class UDirectionalLightComponent;
class UBoxComponent;
class AStaticMeshActor;
class AActor;
class AGameModeBase;
class UWorld;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course", meta = (DisplayName = "课程配置DA", ToolTip = "运行时课程配置；必须与 GameMode 的 CourseConfig 指向同一个 DA_Course。"))
	TObjectPtr<UNightG1CourseConfig> Config;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course", meta = (DisplayName = "备用启动Bootstrap", ToolTip = "未接入 Day 流程时使用的备用启动参数；接入 Day 后路线由 DT_GameStages 传入。"))
	FNightBootstrap Bootstrap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Debug", meta = (DisplayName = "自动开始", ToolTip = "BeginPlay 时是否自动启动 Night 课程。"))
	bool bAutoStart = true;

	/** Use USChefGameInstance's Night → Day inventory and retry flow when present. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Flow", meta = (DisplayName = "使用Day流程", ToolTip = "开启后从 USChefGameInstance 读取 DT_GameStages 的 DefaultRoute/ForkPair/Seed。"))
	bool bUseChefDayFlow = true;

	/** Automatically restart a run after a gameplay failure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Flow", meta = (DisplayName = "失败自动重试", ToolTip = "Night 中途失败后是否自动重新开始。"))
	bool bAutoRetryOnFailure = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Flow", meta = (DisplayName = "自动重试延迟", ToolTip = "失败到自动重试之间的等待时间，单位秒。", ClampMin = "0.05"))
	float AutoRetryDelaySeconds = 0.5f;

	/** Open the configured Day level after a successful Night run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Flow", meta = (DisplayName = "成功后切Day", ToolTip = "Night 成功后是否打开配置的 Day 关卡。"))
	bool bTravelToDayOnSuccess = true;

	/** Primary per-level Day destination. Leave empty to use the GameMode fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Flow", meta = (DisplayName = "成功Day关卡", ToolTip = "Night 成功后要打开的 Day World；为空时使用 GameMode 回退。"))
	TSoftObjectPtr<UWorld> SuccessDayLevel;

	/** Optional GameMode override for the configured Day destination. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Flow", meta = (DisplayName = "成功Day GameMode", ToolTip = "成功进入 Day 时可选的 GameMode 覆盖。"))
	TSoftClassPtr<AGameModeBase> SuccessDayGameMode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Night|Course|Layout")
	TObjectPtr<UBoxComponent> LayoutBounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Layout", meta = (DisplayName = "启用LayoutBounds", ToolTip = "开启后普通 Atom 会检查是否落在 LayoutBounds 内；特殊岔路和分支可按规则绕过。"))
	bool bEnforceLayoutBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Night|Course|Layout", meta = (DisplayName = "LayoutBounds半尺寸", ToolTip = "Bounds 的半尺寸，单位 cm；中心是 Bounds 组件位置。普通课程必须能放入该范围。"))
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
