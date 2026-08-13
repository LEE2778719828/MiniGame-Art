#include "Night/Course/NightCourseStoneActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

#pragma region K2 moonyfli
ANightCourseStoneActor::ANightCourseStoneActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ArtRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArtRoot"));
	SetRootComponent(ArtRoot);

	PlatformMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlatformMesh"));
	PlatformMesh->SetupAttachment(ArtRoot);
	PlatformMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlatformMesh->SetRelativeLocation(FVector(0.f, 0.f, 18.f));
	PlatformMesh->SetRelativeScale3D(FVector(1.8f, 1.8f, 0.35f));

	FoeCapsule = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FoeCapsule"));
	FoeCapsule->SetupAttachment(PlatformMesh);
	FoeCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FoeCapsule->SetRelativeLocation(FVector(0.f, 0.f, 95.f));
	FoeCapsule->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.35f));
	FoeCapsule->SetHiddenInGame(true);
	FoeCapsule->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CapsuleMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CubeMesh.Succeeded())
	{
		PlatformMesh->SetStaticMesh(CubeMesh.Object);
	}
	if (CapsuleMesh.Succeeded())
	{
		FoeCapsule->SetStaticMesh(CapsuleMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitMat(TEXT("/Game/Night/Course/Materials/M_NightUnlitColor.M_NightUnlitColor"));
	if (UnlitMat.Succeeded())
	{
		PlatformMesh->SetMaterial(0, UnlitMat.Object);
		FoeCapsule->SetMaterial(0, UnlitMat.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FadeMat(TEXT("/Game/Night/Course/Materials/M_NightUnlitFade.M_NightUnlitFade"));
	if (FadeMat.Succeeded())
	{
		FadeMaterialParent = FadeMat.Object;
	}
}

void ANightCourseStoneActor::SetupStone(int32 InIndex, const FNightStoneSpec& InSpec)
{
	StoneIndex = InIndex;
	Spec = InSpec;
	bClearingFoe = false;
	FoeClearAlpha = 1.f;
	CurrentFadeOpacity = 1.f;

	const bool bShowFoe = Spec.bHasFoe;
	FoeCapsule->SetHiddenInGame(!bShowFoe);
	FoeCapsule->SetVisibility(bShowFoe);
	FoeCapsule->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.35f));
	ApplyColors();
}

void ANightCourseStoneActor::SetTrackPose(const FVector& WorldLocation, const FRotator& WorldRotation)
{
	SetActorLocationAndRotation(WorldLocation, WorldRotation);
}

void ANightCourseStoneActor::SetHighlight(bool bHighlight)
{
	ApplyColors();
	if (bHighlight)
	{
		TintMesh(PlatformMesh, FLinearColor(1.f, 0.85f, 0.2f));
		if (PlatformMid && bFadeMaterialConfigured)
		{
			PushFadeToMid(PlatformMid, CurrentFadeOpacity, CachedFadeSettings);
		}
	}
}

void ANightCourseStoneActor::ClearFoe(bool bAnimate)
{
	if (!Spec.bHasFoe && (!FoeCapsule || FoeCapsule->bHiddenInGame))
	{
		return;
	}

	Spec.bHasFoe = false;
	PlayFoeClearedVFX();
	PlaySlashVFX();

	if (!bAnimate)
	{
		FoeCapsule->SetHiddenInGame(true);
		FoeCapsule->SetVisibility(false);
		if (!bClearingFoe)
		{
			SetActorTickEnabled(false);
		}
		return;
	}

	bClearingFoe = true;
	FoeClearAlpha = 1.f;
	SetActorTickEnabled(true);
}

void ANightCourseStoneActor::ConfigureDistanceFadeMaterial(UMaterialInterface* FadeMaterial, const FNightDistanceFadeSettings& Settings)
{
	CachedFadeSettings = Settings;
	if (FadeMaterial)
	{
		FadeMaterialParent = FadeMaterial;
	}
	bFadeMaterialConfigured = (FadeMaterialParent != nullptr) && Settings.bEnabled;
	EnsureMeshMids();
	ApplyColors();
	ApplyDistanceFade(CurrentFadeOpacity, Settings);
}

void ANightCourseStoneActor::EnsureMeshMids()
{
	UMaterialInterface* Parent = FadeMaterialParent;
	if (!Parent && PlatformMesh)
	{
		Parent = PlatformMesh->GetMaterial(0);
	}
	if (!Parent)
	{
		return;
	}

	if (PlatformMesh)
	{
		if (bFadeMaterialConfigured && FadeMaterialParent)
		{
			PlatformMid = PlatformMesh->CreateDynamicMaterialInstance(0, FadeMaterialParent);
		}
		else
		{
			PlatformMid = PlatformMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, PlatformMesh->GetMaterial(0));
		}
	}
	if (FoeCapsule)
	{
		if (bFadeMaterialConfigured && FadeMaterialParent)
		{
			FoeMid = FoeCapsule->CreateDynamicMaterialInstance(0, FadeMaterialParent);
		}
		else
		{
			FoeMid = FoeCapsule->CreateAndSetMaterialInstanceDynamicFromMaterial(0, FoeCapsule->GetMaterial(0));
		}
	}
}

void ANightCourseStoneActor::PushFadeToMid(UMaterialInstanceDynamic* Mid, float Opacity01, const FNightDistanceFadeSettings& Settings)
{
	if (!Mid)
	{
		return;
	}
	const float Clamped = FMath::Clamp(Opacity01, 0.f, 1.f);
	if (!Settings.OpacityParamName.IsNone())
	{
		Mid->SetScalarParameterValue(Settings.OpacityParamName, Clamped);
	}
	if (!Settings.FadeAlphaParamName.IsNone())
	{
		Mid->SetScalarParameterValue(Settings.FadeAlphaParamName, Clamped);
	}
}

void ANightCourseStoneActor::ApplyDistanceFade(float Opacity01, const FNightDistanceFadeSettings& Settings)
{
	CachedFadeSettings = Settings;
	CurrentFadeOpacity = FMath::Clamp(Opacity01, 0.f, 1.f);

	if (Settings.bAffectPlatform)
	{
		PushFadeToMid(PlatformMid, CurrentFadeOpacity, Settings);
	}
	if (Settings.bAffectFoe)
	{
		PushFadeToMid(FoeMid, CurrentFadeOpacity, Settings);
	}

	const bool bShouldHide = Settings.bHideWhenBelowThreshold
		&& CurrentFadeOpacity <= Settings.HideBelowOpacity;
	SetActorHiddenInGame(bShouldHide);
	SetActorEnableCollision(!bShouldHide);
}

void ANightCourseStoneActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bClearingFoe || !FoeCapsule)
	{
		return;
	}

	FoeClearAlpha -= DeltaSeconds * 2.5f;
	const float Alpha = FMath::Clamp(FoeClearAlpha, 0.f, 1.f);
	FoeCapsule->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.35f) * Alpha);
	FoeCapsule->AddLocalRotation(FRotator(0.f, 0.f, 360.f * DeltaSeconds));

	if (Alpha <= 0.01f)
	{
		FoeCapsule->SetHiddenInGame(true);
		FoeCapsule->SetVisibility(false);
		bClearingFoe = false;
		SetActorTickEnabled(false);
	}
}

void ANightCourseStoneActor::ApplyColors()
{
	TintMesh(PlatformMesh, PadColor);
	if (FoeCapsule && (FoeCapsule->IsVisible() || Spec.bHasFoe))
	{
		TintMesh(FoeCapsule, FoeColor);
	}
	if (bFadeMaterialConfigured)
	{
		PushFadeToMid(PlatformMid, CurrentFadeOpacity, CachedFadeSettings);
		PushFadeToMid(FoeMid, CurrentFadeOpacity, CachedFadeSettings);
	}
}

void ANightCourseStoneActor::TintMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color)
{
	if (!Mesh)
	{
		return;
	}

	UMaterialInstanceDynamic* Mid = nullptr;
	if (Mesh == PlatformMesh)
	{
		if (!PlatformMid)
		{
			EnsureMeshMids();
		}
		Mid = PlatformMid;
	}
	else if (Mesh == FoeCapsule)
	{
		if (!FoeMid)
		{
			EnsureMeshMids();
		}
		Mid = FoeMid;
	}

	if (!Mid)
	{
		UMaterialInterface* Base = Mesh->GetMaterial(0);
		if (!Base)
		{
			return;
		}
		Mid = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Base);
		if (Mesh == PlatformMesh)
		{
			PlatformMid = Mid;
		}
		else if (Mesh == FoeCapsule)
		{
			FoeMid = Mid;
		}
	}

	if (Mid)
	{
		const FName ColorName = bFadeMaterialConfigured ? CachedFadeSettings.ColorParamName : FName(TEXT("Color"));
		Mid->SetVectorParameterValue(ColorName.IsNone() ? FName(TEXT("Color")) : ColorName, Color);
	}
}
#pragma endregion K2 moonyfli
