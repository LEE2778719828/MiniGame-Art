#include "Night/Course/NightCourseHUD.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCourseTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

#pragma region K2 moonyfli
namespace NightHudPads
{
	// Fractions of the viewport. DrawHUD and the pointer hit-test must stay on the same numbers
	// or a tap that looks like it hit a pad would miss (or the reverse).
	constexpr float JumpX = 0.08f;
	constexpr float AttackX = 0.56f;
	constexpr float ButtonY = 0.84f;
	constexpr float ButtonW = 0.36f;
	constexpr float ButtonH = 0.10f;

	bool Contains(float ScreenX, float ScreenY, float ViewX, float ViewY, float NX, float NY, float NW, float NH)
	{
		if (ViewX <= KINDA_SMALL_NUMBER || ViewY <= KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const float X = NX * ViewX;
		const float Y = NY * ViewY;
		return ScreenX >= X && ScreenX <= X + NW * ViewX && ScreenY >= Y && ScreenY <= Y + NH * ViewY;
	}
}

bool ANightCourseHUD::HitTestActionButtons(
	float ScreenX,
	float ScreenY,
	float ViewX,
	float ViewY,
	ENightFeelInput& OutInput)
{
	using namespace NightHudPads;
	if (Contains(ScreenX, ScreenY, ViewX, ViewY, JumpX, ButtonY, ButtonW, ButtonH))
	{
		OutInput = ENightFeelInput::Jump;
		return true;
	}
	if (Contains(ScreenX, ScreenY, ViewX, ViewY, AttackX, ButtonY, ButtonW, ButtonH))
	{
		OutInput = ENightFeelInput::Attack;
		return true;
	}
	return false;
}

void ANightCourseHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;

	using namespace NightHudPads;
	// Bottom control bar (portrait-friendly).
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.72f), 0.f, H * 0.78f, W, H * 0.22f);
	DrawRect(FLinearColor(0.15f, 0.55f, 0.95f, 0.9f), JumpX * W, ButtonY * H, ButtonW * W, ButtonH * H);
	DrawRect(FLinearColor(0.9f, 0.22f, 0.18f, 0.9f), AttackX * W, ButtonY * H, ButtonW * W, ButtonH * H);

	FCanvasTextItem JumpText(FVector2D(W * 0.14f, H * 0.87f), FText::FromString(TEXT("Q  JUMP")), GEngine->GetLargeFont(), FLinearColor::White);
	JumpText.Scale = FVector2D(1.4f, 1.4f);
	Canvas->DrawItem(JumpText);

	FCanvasTextItem AttackText(FVector2D(W * 0.62f, H * 0.87f), FText::FromString(TEXT("E  ATTACK")), GEngine->GetLargeFont(), FLinearColor::White);
	AttackText.Scale = FVector2D(1.4f, 1.4f);
	Canvas->DrawItem(AttackText);

	float Soul = 100.f;
	FString Prompt;
	FLinearColor PromptColor = FLinearColor(1.f, 0.9f, 0.3f);
	if (APawn* Pawn = GetOwningPawn())
	{
		if (ANightCoursePawn* CoursePawn = Cast<ANightCoursePawn>(Pawn))
		{
			if (UNightFeelStubComponent* Feel = CoursePawn->FeelStub)
			{
				Soul = Feel->Soul;
				if (Feel->bHasActiveRequest)
				{
					const bool bAttack = (Feel->ActiveRequest.Kind == ENightNodeKind::Enemy);
					Prompt = bAttack ? TEXT("NOW: ATTACK") : TEXT("NOW: JUMP");
					PromptColor = bAttack ? FLinearColor(1.f, 0.3f, 0.25f) : FLinearColor(0.3f, 0.75f, 1.f);
				}
			}
		}
	}

	FCanvasTextItem SoulText(FVector2D(W * 0.06f, H * 0.05f), FText::FromString(FString::Printf(TEXT("SOUL  %.0f"), Soul)), GEngine->GetLargeFont(), FLinearColor(1.f, 0.95f, 0.7f));
	SoulText.Scale = FVector2D(1.6f, 1.6f);
	Canvas->DrawItem(SoulText);

	if (!Prompt.IsEmpty())
	{
		FCanvasTextItem PromptText(FVector2D(W * 0.28f, H * 0.12f), FText::FromString(Prompt), GEngine->GetLargeFont(), PromptColor);
		PromptText.Scale = FVector2D(2.0f, 2.0f);
		Canvas->DrawItem(PromptText);
	}
}
#pragma endregion K2 moonyfli
