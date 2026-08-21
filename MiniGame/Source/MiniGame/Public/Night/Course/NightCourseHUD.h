#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Night/Course/NightFeelBridge.h"
#include "NightCourseHUD.generated.h"

#pragma region K2 moonyfli
/** Simple readable G1 HUD: soul, window prompt, key hints. */
UCLASS()
class MINIGAME_API ANightCourseHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/**
	 * add by K2 (R1): maps a screen point onto the two on-screen Jump / Attack pads.
	 * The pads are the same rectangles DrawHUD paints; false means the pointer missed both.
	 */
	static bool HitTestActionButtons(
		float ScreenX,
		float ScreenY,
		float ViewX,
		float ViewY,
		ENightFeelInput& OutInput);
};
#pragma endregion K2 moonyfli
