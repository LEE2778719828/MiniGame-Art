#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Night/Course/NightCourseTypes.h"
#include "NightProcParamsAsset.generated.h"

class UStaticMesh;

#pragma region K2 moonyfli
/** DataAsset mirror of HTML/JSON procedural params + optional art mesh binds. */
UCLASS(BlueprintType)
class MINIGAME_API UNightProcParamsAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Proc")
	FNightProcCourseParams Params;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> BridgeMeshA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> BridgeMeshB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> HeroMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> FoeMeshM01;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> FoeMeshM02;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Art")
	TSoftObjectPtr<UStaticMesh> FoeMeshM03;

	/** Optional baked stones from HTML Export Baked Course. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Proc|Baked")
	TArray<FNightStoneSpec> BakedStones;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Proc|Baked")
	TArray<FNightBeatSpec> BakedBeats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Proc|Baked")
	TArray<FNightBridgeSpec> BakedBridges;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Night|Proc|Baked")
	bool bPreferBakedCourse = false;

	UFUNCTION(BlueprintCallable, Category = "Night|Proc")
	bool ImportFromJsonString(const FString& JsonText, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Night|Proc")
	bool ImportFromJsonFile(const FString& AbsoluteOrProjectPath, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Night|Proc")
	FString ExportToJsonString(bool bIncludeBaked) const;

	UFUNCTION(BlueprintCallable, Category = "Night|Proc")
	static UNightProcParamsAsset* CreateTransientFromJson(const FString& JsonText, FString& OutError);
};
#pragma endregion K2 moonyfli
