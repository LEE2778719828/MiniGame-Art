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
class UDirectionalLightComponent;

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
