#include "Night/Course/NightCourseHUD.h"
#include "Night/Course/NightCoursePawn.h"
#include "Night/Course/NightFeelStubComponent.h"
#include "Night/Course/NightCourseTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

#pragma region K2 moonyfli
void ANightCourseHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const float W = Canvas->SizeX;
	const float H = Canvas->SizeY;

	// Bottom control bar (portrait-friendly).
	DrawRect(FLinearColor(0.02f, 0.03f, 0.06f, 0.72f), 0.f, H * 0.78f, W, H * 0.22f);
	DrawRect(FLinearColor(0.15f, 0.55f, 0.95f, 0.9f), W * 0.08f, H * 0.84f, W * 0.36f, H * 0.1f);
	DrawRect(FLinearColor(0.9f, 0.22f, 0.18f, 0.9f), W * 0.56f, H * 0.84f, W * 0.36f, H * 0.1f);

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
