#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Night/Course/NightCourseTypes.h"
#include "NightFeelBridge.generated.h"

#pragma region K2 moonyfli
UENUM(BlueprintType)
enum class ENightFeelInput : uint8
{
	Jump UMETA(DisplayName = "Jump"),
	Attack UMETA(DisplayName = "Attack")
};

UINTERFACE(BlueprintType)
class MINIGAME_API UNightFeelBridge : public UInterface
{
	GENERATED_BODY()
};

/** R1 Feel contract used by R2 Course. G1 ships a stub; R1 replaces later. */
class MINIGAME_API INightFeelBridge
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Feel")
	void NotifyJudgeRequest(const FNightJudgeRequest& Request);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Feel")
	void ClearJudgeRequest(int32 NodeIndex);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Feel")
	ENightJudgeOutcome TryResolveInput(ENightFeelInput Input);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Feel")
	float GetSoul() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Feel")
	void ApplySoulPenalty(float Amount, ENightJudgeOutcome Reason);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Feel")
	void PlaySuccessFeedback(ENightNodeKind Kind);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Feel")
	void PlayFailFeedback(ENightJudgeOutcome Outcome, ENightNodeKind Kind);

	/** R2 asks R1 to remap Jump/Attack physical mapping (G3 key swap). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Feel")
	void SetControlScheme(ENightControlScheme Scheme);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Night|Feel")
	ENightControlScheme GetControlScheme() const;
};
#pragma endregion K2 moonyfli
