#pragma once

#include "CoreMinimal.h"
#include "Night/Course/NightTrackNodeActor.h"
#include "NightHazardActor.generated.h"

#pragma region K2 moonyfli
UCLASS(Blueprintable)
class MINIGAME_API ANightHazardActor : public ANightTrackNodeActor
{
	GENERATED_BODY()

public:
	ANightHazardActor();

	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Art")
	void PlayClearVFX();

	UFUNCTION(BlueprintImplementableEvent, Category = "Night|Art")
	void PlayImpactVFX();

	virtual void OnResolved_Implementation(ENightJudgeOutcome Outcome) override;
};
#pragma endregion K2 moonyfli
