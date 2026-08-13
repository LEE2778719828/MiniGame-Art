#pragma once

#include "CoreMinimal.h"
#include "Night/Course/NightTrackNodeActor.h"
#include "NightFoeActor.generated.h"

#pragma region K2 moonyfli
UCLASS(Blueprintable)
class MINIGAME_API ANightFoeActor : public ANightTrackNodeActor
{
	GENERATED_BODY()

public:
	ANightFoeActor();

	/** Niagara / montage hooks for art — call from BP OnResolved(Success). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Art")
	void PlaySlashVFX();

	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Art")
	void PlayDropBurst(EIngredientId DropId, int32 Count);

	virtual void OnResolved_Implementation(ENightJudgeOutcome Outcome) override;
};
#pragma endregion K2 moonyfli
