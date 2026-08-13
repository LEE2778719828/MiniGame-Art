#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "SDayWhiteboxAssetsCommandlet.generated.h"

#pragma region K2 moonyfli

/** One-shot editor commandlet that creates the replaceable Day-board asset shells. */
UCLASS()
class MINIGAME_API USDayWhiteboxAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	USDayWhiteboxAssetsCommandlet();
	virtual int32 Main(const FString& Params) override;
};

#pragma endregion K2 moonyfli
