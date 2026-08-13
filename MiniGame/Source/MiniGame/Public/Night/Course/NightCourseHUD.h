#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "NightCourseHUD.generated.h"

#pragma region K2 moonyfli
/** Simple readable G1 HUD: soul, window prompt, key hints. */
UCLASS()
class MINIGAME_API ANightCourseHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
#pragma endregion K2 moonyfli
