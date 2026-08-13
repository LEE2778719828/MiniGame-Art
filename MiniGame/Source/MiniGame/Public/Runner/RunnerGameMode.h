#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RunnerGameMode.generated.h"

class URunnerTrackData;

#pragma region K2 moonyfli
UCLASS()
class MINIGAME_API ARunnerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARunnerGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner")
	TObjectPtr<URunnerTrackData> DefaultTrackData;

protected:
	virtual void BeginPlay() override;
};
#pragma endregion K2 moonyfli
