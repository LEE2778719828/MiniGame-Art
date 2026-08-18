#include "Day/Presentation/SDayBoardPresentation.h"

#include "../../../SStandaloneSandbox.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/SkyLightComponent.h"
#include "Components/SizeBox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace DayBoardPresentationPrivate
{
	const FName DayLingGuId(TEXT("LingGu"));
	const FName DayYinShanJunId(TEXT("YinShanJun"));
	const FName DayChiYanJiaoId(TEXT("ChiYanJiao"));
	const FName DayYueLinYuId(TEXT("YueLinYu"));
	const FName DayXuanYuQinId(TEXT("XuanYuQin"));
	const FName DayNpcALingId(TEXT("ALing"));
	const FName DayNpcSangPoId(TEXT("SangPo"));

	UStaticMesh* LoadBasicShape(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	FLinearColor IngredientColor(const FName IngredientId)
	{
		if (IngredientId == DayLingGuId) return FLinearColor(0.98f, 0.14f, 0.28f);
		if (IngredientId == DayYinShanJunId) return FLinearColor(0.10f, 0.78f, 0.64f);
		if (IngredientId == DayChiYanJiaoId) return FLinearColor(1.00f, 0.35f, 0.05f);
		if (IngredientId == DayYueLinYuId) return FLinearColor(0.10f, 0.42f, 0.95f);
		if (IngredientId == DayXuanYuQinId) return FLinearColor(0.72f, 0.18f, 0.90f);
		return FLinearColor::White;
	}

	FString IngredientShortName(const UObject* WorldContext, const FName IngredientId)
	{
		if (WorldContext)
		{
			if (const USChefGameInstance* GameInstance = WorldContext->GetWorld()
				? WorldContext->GetWorld()->GetGameInstance<USChefGameInstance>()
				: nullptr)
			{
				const FString Resolved = GameInstance->ResolveIngredientShortName(IngredientId);
				if (!Resolved.IsEmpty())
				{
					return Resolved;
				}
			}
		}
		if (IngredientId == DayLingGuId) return TEXT("灵");
		if (IngredientId == DayYinShanJunId) return TEXT("阴");
		if (IngredientId == DayChiYanJiaoId) return TEXT("赤");
		if (IngredientId == DayYueLinYuId) return TEXT("月");
		if (IngredientId == DayXuanYuQinId) return TEXT("玄");
		return IngredientId.ToString().Left(1);
	}

	void SetButtonText(UButton* Button, const FString& Text)
	{
		if (Button && Button->GetChildrenCount() > 0)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Button->GetChildAt(0)))
			{
				Label->SetText(FText::FromString(Text));
			}
		}
	}

	/** The portrait camera uses Yaw 90 so world +Y is screen-up; mirror X keeps the art's left/right composition. */
	FVector MirrorX(const FVector& In)
	{
		return FVector(-In.X, In.Y, In.Z);
	}

	/**
	 * World text lies flat with its front face toward the portrait camera, so it reads
	 * horizontally (and unmirrored) on a phone screen. Pitch 90 turns the glyph plane up,
	 * Yaw -90 lines the reading direction up with the camera's Yaw 90 framing.
	 */
	FRotator LabelFacingRotation()
	{
		return FRotator(90.0f, -90.0f, 0.0f);
	}

	FLinearColor IdleCellColor()
	{
		return FLinearColor(0.34f, 0.30f, 0.26f);
	}

	FLinearColor SelectedCellColor()
	{
		return FLinearColor(1.00f, 0.84f, 0.25f);
	}

	/** Labels that sit on top of pale meshes need dark text to stay readable. */
	FColor LabelInkColor()
	{
		return FColor(24, 20, 16);
	}

	/** SetFont recomputes the glyph scale, so the world size has to be re-applied afterwards. */
	void ApplyLabelFont(UTextRenderComponent* Label, UFont* Font, const float WorldSize)
	{
		if (!Label || !Font)
		{
			return;
		}
		Label->SetFont(Font);
		Label->SetWorldSize(WorldSize);
	}

	/** Placeholder materials derive from BasicShapeMaterial ("Color"); set both names so future art materials tint too. */
	void ApplyTint(UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (!Mesh)
		{
			return;
		}
		const FVector Rgb(Color.R, Color.G, Color.B);
		Mesh->SetVectorParameterValueOnMaterials(TEXT("Color"), Rgb);
		Mesh->SetVectorParameterValueOnMaterials(TEXT("Tint"), Rgb);
	}
}

using namespace DayBoardPresentationPrivate;

#pragma region K2 moonyfli

ASDayCellVisual::ASDayCellVisual()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	CellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CellMesh"));
	CellMesh->SetupAttachment(Root);
	CellMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CellMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CellMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PieceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PieceMesh"));
	PieceMesh->SetupAttachment(Root);
	PieceMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 32.0f));
	PieceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PieceLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PieceLabel"));
	PieceLabel->SetupAttachment(Root);
	PieceLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f));
	PieceLabel->SetRelativeRotation(LabelFacingRotation());
	PieceLabel->SetHorizontalAlignment(EHTA_Center);
	PieceLabel->SetVerticalAlignment(EVRTA_TextCenter);
	PieceLabel->SetWorldSize(36.0f);
	PieceLabel->SetTextRenderColor(FColor::White);
}

void ASDayCellVisual::Configure(const int32 InCellIndex, const float InRadius, ASMergeBoard* InBoard)
{
	CellIndex = InCellIndex;
	VisualRadius = InRadius;
	Board = InBoard;

	if (!CellMesh->GetStaticMesh())
	{
		CellMesh->SetStaticMesh(LoadBasicShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	}
	if (!PieceMesh->GetStaticMesh())
	{
		PieceMesh->SetStaticMesh(LoadBasicShape(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	}

	const float CellScale = VisualRadius / 50.0f;
	CellMesh->SetRelativeScale3D(FVector(CellScale, CellScale * 0.82f, 0.12f));
	PieceLabel->SetRelativeRotation(LabelFacingRotation());
	RefreshVisual();
}

void ASDayCellVisual::SetLabelFont(UFont* InFont)
{
	ApplyLabelFont(PieceLabel, InFont, 36.0f);
}

void ASDayCellVisual::RefreshVisual()
{
	ASMergeBoard* MergeBoard = Board.Get();
	if (!MergeBoard)
	{
		// A level-placed presenter can build before the gateway spawns the board, so rebind lazily.
		MergeBoard = ASMergeBoard::FindBoard(this);
		Board = MergeBoard;
	}
	if (!MergeBoard)
	{
		PieceMesh->SetVisibility(false);
		PieceLabel->SetVisibility(false);
		return;
	}

	FSDishPiece Piece;
	const bool bHasPiece = MergeBoard->TryGetPiece(CellIndex, Piece);
	const bool bSelected = MergeBoard->GetActiveDragCellIndex() == CellIndex;
	PieceMesh->SetVisibility(bHasPiece);
	PieceLabel->SetVisibility(bHasPiece);
	CellMesh->SetCustomDepthStencilValue(bSelected ? 2 : 1);
	CellMesh->SetRenderCustomDepth(bSelected);
	// The whitebox has no outline post-process, so the selection reads through tint and lift instead.
	ApplyTint(CellMesh, bSelected ? SelectedCellColor() : IdleCellColor());

	if (!bHasPiece)
	{
		return;
	}

	const float SelectionBoost = bSelected ? 1.25f : 1.0f;
	const float LevelScale = (0.52f + static_cast<float>(Piece.Level) * 0.10f) * SelectionBoost;
	PieceMesh->SetRelativeScale3D(FVector(
		LevelScale,
		LevelScale,
		(0.25f + Piece.Level * 0.05f) * SelectionBoost));
	PieceMesh->SetRelativeLocation(FVector(0.0f, 0.0f, bSelected ? 62.0f : 32.0f));

	const FLinearColor PieceColor = IngredientColor(Piece.IngredientId);
	ApplyTint(PieceMesh, bSelected ? PieceColor * 1.8f + FLinearColor(0.15f, 0.15f, 0.05f, 0.0f) : PieceColor);
	PieceLabel->SetRelativeLocation(FVector(0.0f, 0.0f, bSelected ? 108.0f : 75.0f));
	// Ink instead of white/amber: the board and the highlight tint are both pale.
	PieceLabel->SetTextRenderColor(LabelInkColor());
	PieceLabel->SetText(FText::FromString(FString::Printf(
		TEXT("%s%s Lv%d"),
		bSelected ? TEXT("▲ ") : TEXT(""),
		*IngredientShortName(this, Piece.IngredientId),
		Piece.Level)));
}

ASDayIngredientBinVisual::ASDayIngredientBinVisual()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BinMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BinMesh"));
	BinMesh->SetupAttachment(Root);
	BinMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BinMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BinMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	Label->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
	Label->SetRelativeRotation(LabelFacingRotation());
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(28.0f);
}

void ASDayIngredientBinVisual::Configure(const FName InIngredientId, const FString& InDisplayName)
{
	IngredientId = InIngredientId;
	if (!BinMesh->GetStaticMesh())
	{
		BinMesh->SetStaticMesh(LoadBasicShape(TEXT("/Engine/BasicShapes/Cube.Cube")));
	}
	BinMesh->SetRelativeScale3D(FVector(1.15f, 0.72f, 0.55f));
	ApplyTint(BinMesh, IngredientColor(IngredientId));
	Label->SetRelativeRotation(LabelFacingRotation());
	Label->SetText(FText::FromString(InDisplayName));
	// The bin mesh already carries the ingredient colour, so tinting the text to match hides it.
	Label->SetTextRenderColor(LabelInkColor());
}

void ASDayIngredientBinVisual::SetLabelFont(UFont* InFont)
{
	ApplyLabelFont(Label, InFont, 28.0f);
}

ASDayCharacterStandIn::ASDayCharacterStandIn()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	CharacterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CharacterMesh"));
	CharacterMesh->SetupAttachment(Root);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CharacterMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CharacterMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Root);
	// -Y is screen-down under the portrait camera; the seats hug the top edge, so the
	// name plate goes underneath them where there is empty space.
	Label->SetRelativeLocation(FVector(0.0f, -120.0f, 150.0f));
	Label->SetRelativeRotation(LabelFacingRotation());
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(24.0f);
}

void ASDayCharacterStandIn::Configure(const FString& InLabel, const FLinearColor& InColor)
{
	if (!CharacterMesh->GetStaticMesh())
	{
		CharacterMesh->SetStaticMesh(LoadBasicShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	}
	CharacterMesh->SetRelativeScale3D(FVector(0.62f, 0.62f, 1.25f));
	// Live Coding keeps existing instances, so re-apply pointer settings outside the constructor.
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CharacterMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CharacterMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ApplyTint(CharacterMesh, InColor);
	Label->SetRelativeLocation(FVector(0.0f, -120.0f, 150.0f));
	Label->SetRelativeRotation(LabelFacingRotation());
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(24.0f);
	SetHeadline(InLabel, InColor);
}

void ASDayCharacterStandIn::SetHeadline(const FString& InText, const FLinearColor& InColor)
{
	if (!Label)
	{
		return;
	}
	Label->SetText(FText::FromString(InText));
	Label->SetTextRenderColor(InColor.ToFColor(true));
}

void ASDayCharacterStandIn::SetLabelFont(UFont* InFont)
{
	ApplyLabelFont(Label, InFont, 24.0f);
}

ASDayBoardPresenter::ASDayBoardPresenter()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BoardFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardFrame"));
	BoardFrame->SetupAttachment(Root);
	BoardFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Counter = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Counter"));
	Counter->SetupAttachment(Root);
	Counter->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("PortraitCamera"));
	Camera->SetupAttachment(Root);
	Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
	Camera->SetRelativeLocation(FVector(0.0f, 60.0f, 2200.0f));
	// Yaw 90 maps world +Y to screen-up so the tall board axis fills the portrait frame.
	Camera->SetRelativeRotation(FRotator(-90.0f, 90.0f, 0.0f));
	Camera->OrthoWidth = PortraitOrthoWidth;
	Camera->bConstrainAspectRatio = true;
	Camera->AspectRatio = 9.0f / 16.0f;

	WhiteboxKeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("WhiteboxKeyLight"));
	WhiteboxKeyLight->SetupAttachment(Root);
	WhiteboxKeyLight->SetRelativeRotation(FRotator(-55.0f, -35.0f, 0.0f));
	WhiteboxKeyLight->SetIntensity(7.0f);
	WhiteboxKeyLight->SetLightColor(FLinearColor(1.0f, 0.90f, 0.78f));

	WhiteboxFillLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("WhiteboxFillLight"));
	WhiteboxFillLight->SetupAttachment(Root);
	WhiteboxFillLight->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap;
	WhiteboxFillLight->SetIntensity(1.5f);
	WhiteboxFillLight->SetLightColor(FLinearColor(0.55f, 0.68f, 1.0f));

	VisualConfig = TSoftObjectPtr<USDayBoardVisualConfig>(
		FSoftObjectPath(TEXT("/Game/Day/Data/DA_SDayBoardVisualConfig.DA_SDayBoardVisualConfig")));
}

void ASDayBoardPresenter::BeginPlay()
{
	Super::BeginPlay();
	LogicBoard = ASMergeBoard::FindBoard(this);
	BuildWhitebox();

	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->OnSandboxStateChanged.AddUniqueDynamic(this, &ASDayBoardPresenter::RefreshFromLogic);
	}

	if (bTakeCameraOnBeginPlay)
	{
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
		{
			// Re-apply the portrait framing at runtime so Live Coding picks it up without an editor restart.
			Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
			Camera->SetRelativeLocation(FVector(0.0f, 60.0f, 2200.0f));
			Camera->SetRelativeRotation(FRotator(-90.0f, 90.0f, 0.0f));
			Camera->OrthoWidth = PortraitOrthoWidth;
			Camera->bConstrainAspectRatio = true;
			Camera->AspectRatio = 9.0f / 16.0f;
			PlayerController->SetViewTarget(this);
		}
	}
	RefreshFromLogic();
}

void ASDayBoardPresenter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->OnSandboxStateChanged.RemoveDynamic(this, &ASDayBoardPresenter::RefreshFromLogic);
	}
	Super::EndPlay(EndPlayReason);
}

void ASDayBoardPresenter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Spawn and shop countdowns move without board events, so keep the presentation fresh.
	SeatRefreshCountdown -= DeltaSeconds;
	if (SeatRefreshCountdown <= 0.0f)
	{
		SeatRefreshCountdown = 0.25f;
		RefreshFromLogic();
	}

	FVector2D ScreenPosition;
	const bool bPointerDown = GetPointerState(ScreenPosition);
	if (bPointerDown)
	{
		LastPointerPosition = ScreenPosition;
	}

	if (bPointerDown && !bPointerWasDown)
	{
		HandlePointerPressed(ScreenPosition);
	}
	else if (!bPointerDown && bPointerWasDown)
	{
		HandlePointerReleased(LastPointerPosition);
	}
	bPointerWasDown = bPointerDown;
}

void ASDayBoardPresenter::BuildWhitebox()
{
	USDayBoardVisualConfig* Config = VisualConfig.LoadSynchronous();
	UStaticMesh* Cube = LoadBasicShape(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = LoadBasicShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	UMaterialInterface* BoardMaterial = Config ? Config->BoardMaterial.LoadSynchronous() : nullptr;
	UMaterialInterface* CellMaterial = Config ? Config->CellMaterial.LoadSynchronous() : nullptr;

	BoardFrame->SetStaticMesh(Config && Config->BoardFrameMesh.LoadSynchronous()
		? Config->BoardFrameMesh.Get()
		: Cube);
	// Sized to enclose the 12 playable cells (X +/-330, Y -150..450) plus their visual radius.
	BoardFrame->SetRelativeLocation(FVector(0.0f, 150.0f, -25.0f));
	BoardFrame->SetRelativeScale3D(FVector(9.0f, 8.0f, 0.25f));
	if (BoardMaterial)
	{
		BoardFrame->SetMaterial(0, BoardMaterial);
	}

	Counter->SetStaticMesh(Cube);
	Counter->SetRelativeLocation(FVector(0.0f, 760.0f, 65.0f));
	Counter->SetRelativeScale3D(FVector(9.0f, 0.45f, 0.65f));
	if (BoardMaterial)
	{
		Counter->SetMaterial(0, BoardMaterial);
	}

	const TArray<FVector> DecorativeLocations =
	{
		{-390, 470, 15}, {350, 470, 15}, {-400, 350, 15}, {390, 350, 15},
		{-410, 210, 15}, {400, 150, 15}, {-400, -50, 15}, {390, -100, 15},
		{-360, -240, 15}, {350, -260, 15}, {-300, -360, 15}, {270, -390, 15},
		{-20, 390, 15}, {40, -280, 15}, {300, 80, 15}
	};
	for (int32 Index = 0; Index < DecorativeLocations.Num(); ++Index)
	{
		UStaticMeshComponent* Decor = NewObject<UStaticMeshComponent>(
			this, *FString::Printf(TEXT("DecorativeCell_%02d"), Index));
		Decor->SetStaticMesh(Cylinder);
		Decor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Decor->SetRelativeLocation(MirrorX(DecorativeLocations[Index]));
		const float S = 0.65f + static_cast<float>(Index % 4) * 0.10f;
		Decor->SetRelativeScale3D(FVector(S, S * 0.75f, 0.08f));
		Decor->SetupAttachment(RootComponent);
		Decor->RegisterComponent();
		if (CellMaterial)
		{
			Decor->SetMaterial(0, CellMaterial);
		}
	}

	BuildCells();
	BuildBins();
	BuildCharacters();
}

TArray<FSDayBoardLayoutRow> ASDayBoardPresenter::GetLayoutRows() const
{
	TArray<FSDayBoardLayoutRow> Rows;
	const USDayBoardVisualConfig* Config = VisualConfig.Get();
	if (Config)
	{
		if (const UDataTable* Layout = Config->CellLayout.LoadSynchronous())
		{
			TArray<FSDayBoardLayoutRow*> TableRows;
			Layout->GetAllRows(TEXT("SDayBoardPresenter"), TableRows);
			for (const FSDayBoardLayoutRow* Row : TableRows)
			{
				if (Row)
				{
					Rows.Add(*Row);
				}
			}
		}
	}

	if (Rows.Num() > 0)
	{
		Rows.Sort([](const FSDayBoardLayoutRow& A, const FSDayBoardLayoutRow& B)
		{
			return A.CellIndex < B.CellIndex;
		});
		return Rows;
	}

	const TArray<int32> CellIndices = {1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14};
	const TArray<FVector> Locations =
	{
		{-185, 450, 35}, {150, 445, 35}, {-285, 300, 35}, {-65, 290, 35},
		{175, 300, 35}, {330, 230, 35}, {-305, 105, 35}, {-75, 105, 35},
		{165, 100, 35}, {320, -25, 35}, {-165, -125, 35}, {125, -150, 35}
	};
	for (int32 Index = 0; Index < CellIndices.Num(); ++Index)
	{
		FSDayBoardLayoutRow Row;
		Row.CellIndex = CellIndices[Index];
		Row.Transform = FTransform(FRotator::ZeroRotator, Locations[Index], FVector::OneVector);
		Row.VisualRadius = Index % 3 == 0 ? 92.0f : 78.0f;
		Rows.Add(Row);
	}
	return Rows;
}

void ASDayBoardPresenter::BuildCells()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	USDayBoardVisualConfig* Config = VisualConfig.Get();
	TSubclassOf<ASDayCellVisual> VisualClass = ASDayCellVisual::StaticClass();
	if (Config && Config->CellVisualClass)
	{
		VisualClass = Config->CellVisualClass;
	}

	for (const FSDayBoardLayoutRow& Row : GetLayoutRows())
	{
		FTransform LocalTransform = Row.Transform;
		LocalTransform.SetLocation(MirrorX(Row.Transform.GetLocation()));

		FActorSpawnParameters Params;
		Params.Owner = this;
		ASDayCellVisual* Visual = World->SpawnActor<ASDayCellVisual>(
			VisualClass,
			LocalTransform * GetActorTransform(),
			Params);
		if (!Visual)
		{
			continue;
		}
		Visual->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		// Assign meshes/materials before Configure so the tint pass binds to the placeholder material.
		if (Config)
		{
			if (UStaticMesh* Mesh = Config->CellMesh.LoadSynchronous())
			{
				Visual->CellMesh->SetStaticMesh(Mesh);
			}
			if (UStaticMesh* Mesh = Config->PieceMesh.LoadSynchronous())
			{
				Visual->PieceMesh->SetStaticMesh(Mesh);
			}
			if (UMaterialInterface* Material = Config->CellMaterial.LoadSynchronous())
			{
				Visual->CellMesh->SetMaterial(0, Material);
			}
			if (UMaterialInterface* Material = Config->PieceMaterial.LoadSynchronous())
			{
				Visual->PieceMesh->SetMaterial(0, Material);
			}
		}
		Visual->SetLabelFont(ResolveLabelFont());
		Visual->Configure(Row.CellIndex, Row.VisualRadius, LogicBoard.Get());
		CellVisuals.Add(Visual);
	}
}

void ASDayBoardPresenter::BuildBins()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	USDayBoardVisualConfig* Config = VisualConfig.Get();
	TSubclassOf<ASDayIngredientBinVisual> BinClass = ASDayIngredientBinVisual::StaticClass();
	if (Config && Config->IngredientBinClass)
	{
		BinClass = Config->IngredientBinClass;
	}
	const TArray<FName> Ids = {DayLingGuId, DayYinShanJunId, DayChiYanJiaoId, DayYueLinYuId, DayXuanYuQinId};

	for (int32 Index = 0; Index < Ids.Num(); ++Index)
	{
		const FVector Location(-380.0f + Index * 190.0f, -690.0f + FMath::Abs(2 - Index) * 18.0f, 45.0f);
		FActorSpawnParameters Params;
		Params.Owner = this;
		ASDayIngredientBinVisual* Bin = World->SpawnActor<ASDayIngredientBinVisual>(
			BinClass,
			FTransform(MirrorX(Location)) * GetActorTransform(),
			Params);
		if (!Bin)
		{
			continue;
		}
		Bin->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		if (Config)
		{
			if (UStaticMesh* Mesh = Config->IngredientBinMesh.LoadSynchronous())
			{
				Bin->BinMesh->SetStaticMesh(Mesh);
			}
			if (UMaterialInterface* Material = Config->PieceMaterial.LoadSynchronous())
			{
				Bin->BinMesh->SetMaterial(0, Material);
			}
		}
		Bin->SetLabelFont(ResolveLabelFont());
		Bin->Configure(Ids[Index], IngredientShortName(this, Ids[Index]));
		IngredientBins.Add(Bin);
	}
}

void ASDayBoardPresenter::BuildCharacters()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	USDayBoardVisualConfig* Config = VisualConfig.Get();
	TSubclassOf<ASDayCharacterStandIn> CharacterClass = ASDayCharacterStandIn::StaticClass();
	if (Config && Config->CharacterStandInClass)
	{
		CharacterClass = Config->CharacterStandInClass;
	}
#pragma region K2 moonyfli
	// Shared seats along the top plus the chef; occupants are assigned at refresh time.
	int32 DeliverySeatCount = 4;
	if (const USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		DeliverySeatCount = GameInstance->GetServiceSeatCount();
	}
	DeliverySeatCount = FMath::Clamp(DeliverySeatCount, 1, 6);

	TArray<FString> Labels;
	TArray<FVector> Locations;
	TArray<FLinearColor> Colors;
	Labels.Reserve(DeliverySeatCount + 1);
	Locations.Reserve(DeliverySeatCount + 1);
	Colors.Reserve(DeliverySeatCount + 1);

	constexpr float SeatLeftX = -315.0f;
	constexpr float SeatRightX = 315.0f;
	constexpr float SeatY = 815.0f;
	constexpr float SeatZ = 105.0f;
	for (int32 SeatIndex = 0; SeatIndex < DeliverySeatCount; ++SeatIndex)
	{
		const float Alpha = DeliverySeatCount == 1
			? 0.5f
			: static_cast<float>(SeatIndex) / static_cast<float>(DeliverySeatCount - 1);
		Labels.Add(TEXT("空座"));
		Locations.Add(FVector(FMath::Lerp(SeatLeftX, SeatRightX, Alpha), SeatY, SeatZ));
		Colors.Add(FLinearColor(0.95f, 0.75f, 0.65f));
	}
	Labels.Add(TEXT("厨师"));
	Locations.Add(FVector(405.0f, -365.0f, 105.0f));
	Colors.Add(FLinearColor(0.92f, 0.92f, 0.88f));
	const int32 ChefIndex = DeliverySeatCount;
#pragma endregion K2 moonyfli

	for (int32 Index = 0; Index < Labels.Num(); ++Index)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		ASDayCharacterStandIn* Character = World->SpawnActor<ASDayCharacterStandIn>(
			CharacterClass,
			FTransform(MirrorX(Locations[Index])) * GetActorTransform(),
			Params);
		if (!Character)
		{
			continue;
		}
		Character->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
		if (Config)
		{
			UStaticMesh* Mesh = Index == ChefIndex
				? Config->ChefMesh.LoadSynchronous()
				: Config->CustomerMesh.LoadSynchronous();
			if (Mesh)
			{
				Character->CharacterMesh->SetStaticMesh(Mesh);
			}
			if (UMaterialInterface* Material = Config->PieceMaterial.LoadSynchronous())
			{
				Character->CharacterMesh->SetMaterial(0, Material);
			}
		}
		Character->NpcId = NAME_None;
		Character->CustomerId.Reset();
		Character->SeatIndex = Index < ChefIndex ? Index : INDEX_NONE;
		Character->bOccupied = false;
		Character->bDeliveryTarget = Index != ChefIndex;
		Character->SetLabelFont(ResolveLabelFont());
		Character->Configure(Labels[Index], Colors[Index]);
		CharacterStandIns.Add(Character);
	}

	RefreshCharacters();
}

UFont* ASDayBoardPresenter::ResolveLabelFont() const
{
	if (const USDayBoardVisualConfig* Config = VisualConfig.Get())
	{
		if (UFont* Font = Config->LabelFont.LoadSynchronous())
		{
			return Font;
		}
	}
	// Fall back to the generated asset so labels stay readable before the config is re-saved.
	return LoadObject<UFont>(nullptr, TEXT("/Game/Day/UI/F_SDayLabel.F_SDayLabel"));
}

void ASDayBoardPresenter::RefreshCharacters()
{
	const ASCustomerDirector* CustomerDirector = ASCustomerDirector::FindDirector(this);
	const ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this);

	TArray<ASDayCharacterStandIn*> Seats;
	for (ASDayCharacterStandIn* Character : CharacterStandIns)
	{
		if (Character && Character->bDeliveryTarget)
		{
			Seats.Add(Character);
		}
	}
	if (Seats.IsEmpty())
	{
		return;
	}

	for (int32 SeatIndex = 0; SeatIndex < Seats.Num(); ++SeatIndex)
	{
		ASDayCharacterStandIn* Seat = Seats[SeatIndex];
		Seat->SeatIndex = SeatIndex;
		Seat->NpcId = NAME_None;
		Seat->CustomerId.Reset();
		Seat->bOccupied = false;

#pragma region K2 moonyfli
		FSCustomerState Customer;
		if (CustomerDirector && CustomerDirector->TryGetCustomerAtSeat(SeatIndex, Customer))
		{
			Seat->CustomerId = Customer.CustomerId;
			Seat->bOccupied = true;
			ApplyTint(Seat->CharacterMesh, FLinearColor(0.95f, 0.75f, 0.65f));
			Seat->SetHeadline(
				FString::Printf(
					TEXT("%s\n%sLv%d\n等待中"),
					Customer.DisplayName.IsEmpty() ? *Customer.CustomerId : *Customer.DisplayName,
					*IngredientShortName(this, Customer.Order.IngredientId),
					Customer.Order.Level),
				FLinearColor(0.98f, 0.86f, 0.42f));
			continue;
		}

		FSSpecialNpcState SeatedNpc;
		bool bHasNpc = false;
		if (NpcDirector)
		{
			for (const FSSpecialNpcState& Npc : NpcDirector->GetNpcs())
			{
				if (Npc.bPresent && !Npc.bServed && Npc.SeatIndex == SeatIndex)
				{
					SeatedNpc = Npc;
					bHasNpc = true;
					break;
				}
			}
		}

		if (bHasNpc)
		{
			Seat->NpcId = SeatedNpc.NpcId;
			Seat->bOccupied = true;
			ApplyTint(Seat->CharacterMesh, FLinearColor(0.20f, 0.85f, 0.70f));
			Seat->SetHeadline(
				FString::Printf(
					TEXT("%s\n%sLv%d\n→%s"),
					*SeatedNpc.DisplayName,
					*IngredientShortName(this, SeatedNpc.Order.IngredientId),
					SeatedNpc.Order.Level,
					*USChefGameInstance::GetGiftDisplayName(SeatedNpc.GiftId)),
				FLinearColor(0.40f, 0.95f, 0.82f));
			continue;
		}

		ApplyTint(Seat->CharacterMesh, FLinearColor(0.55f, 0.53f, 0.50f));
		const float Cooldown = CustomerDirector
			? CustomerDirector->GetSeatCooldownRemaining(SeatIndex)
			: 0.0f;
		Seat->SetHeadline(
			FString::Printf(TEXT("空座%d\n补客 %.0fs"), SeatIndex + 1, Cooldown),
			FLinearColor(0.62f, 0.58f, 0.54f));
#pragma endregion K2 moonyfli
	}
}

bool ASDayBoardPresenter::TryDeliverToCharacter(ASDayCharacterStandIn* Character, ASMergeBoard* Board)
{
	if (!Character || !Character->bDeliveryTarget || !Board || !Board->IsDragging())
	{
		return false;
	}

	if (!Character->bOccupied) //add by K2
	{
		return false;
	}

	if (Character->NpcId.IsNone())
	{
		if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
		{
			Director->TryDeliverFromCellToCustomer(
				Board->GetActiveDragCellIndex(),
				Character->CustomerId);
			return true;
		}
		return false;
	}

	if (ASSpecialNpcDirector* Director = ASSpecialNpcDirector::FindDirector(this))
	{
		Director->TryDeliverToNpc(Character->NpcId);
		return true;
	}
	return false;
}

void ASDayBoardPresenter::RefreshFromLogic()
{
	if (!LogicBoard.IsValid())
	{
		LogicBoard = ASMergeBoard::FindBoard(this);
	}
	for (ASDayCellVisual* Visual : CellVisuals)
	{
		if (Visual)
		{
			Visual->RefreshVisual();
		}
	}

#pragma region K2 moonyfli
	int32 DesiredSeats = 2;
	if (const USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		DesiredSeats = GameInstance->GetServiceSeatCount();
	}
	DesiredSeats = FMath::Clamp(DesiredSeats, 1, 6);
	if (GetDeliverySeatCount() != DesiredSeats)
	{
		for (ASDayCharacterStandIn* Character : CharacterStandIns)
		{
			if (Character)
			{
				Character->Destroy();
			}
		}
		CharacterStandIns.Reset();
		BuildCharacters();
	}
#pragma endregion K2 moonyfli

	RefreshCharacters();
}

void ASDayBoardPresenter::SimulatePointerEvent(const FVector2D ScreenPosition, const bool bPressed)
{
	if (bPressed)
	{
		HandlePointerPressed(ScreenPosition);
	}
	else
	{
		HandlePointerReleased(ScreenPosition);
	}
}

ASDayCellVisual* ASDayBoardPresenter::GetCellVisual(const int32 CellIndex) const
{
	for (ASDayCellVisual* Visual : CellVisuals)
	{
		if (Visual && Visual->CellIndex == CellIndex)
		{
			return Visual;
		}
	}
	return nullptr;
}

ASDayCharacterStandIn* ASDayBoardPresenter::GetSeat(const FName InNpcId) const
{
	for (ASDayCharacterStandIn* Character : CharacterStandIns)
	{
		if (Character && Character->bDeliveryTarget && Character->bOccupied && Character->NpcId == InNpcId)
		{
			return Character;
		}
	}
	return nullptr;
}

#pragma region K2 moonyfli
int32 ASDayBoardPresenter::GetDeliverySeatCount() const
{
	int32 Count = 0;
	for (const ASDayCharacterStandIn* Character : CharacterStandIns)
	{
		if (Character && Character->bDeliveryTarget)
		{
			++Count;
		}
	}
	return Count;
}

ASDayIngredientBinVisual* ASDayBoardPresenter::GetIngredientBin(const FName IngredientId) const
{
	for (ASDayIngredientBinVisual* Bin : IngredientBins)
	{
		if (Bin && Bin->IngredientId == IngredientId)
		{
			return Bin;
		}
	}
	return nullptr;
}

bool ASDayBoardPresenter::IsInIngredientDropZone(const FVector2D& ScreenPosition) const
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController || IngredientBins.IsEmpty())
	{
		return false;
	}

	FVector2D Min(FLT_MAX, FLT_MAX);
	FVector2D Max(-FLT_MAX, -FLT_MAX);
	int32 ProjectedCount = 0;
	for (const ASDayIngredientBinVisual* Bin : IngredientBins)
	{
		FVector2D BinScreen;
		if (Bin && UGameplayStatics::ProjectWorldToScreen(
			PlayerController,
			Bin->GetActorLocation(),
			BinScreen))
		{
			Min.X = FMath::Min(Min.X, BinScreen.X);
			Min.Y = FMath::Min(Min.Y, BinScreen.Y);
			Max.X = FMath::Max(Max.X, BinScreen.X);
			Max.Y = FMath::Max(Max.Y, BinScreen.Y);
			++ProjectedCount;
		}
	}

	if (ProjectedCount == 0)
	{
		return false;
	}

	// A generous shared rectangle covers all five baskets and the gaps between them.
	const FVector2D Padding(120.0f, 100.0f);
	return ScreenPosition.X >= Min.X - Padding.X
		&& ScreenPosition.X <= Max.X + Padding.X
		&& ScreenPosition.Y >= Min.Y - Padding.Y
		&& ScreenPosition.Y <= Max.Y + Padding.Y;
}

bool ASDayBoardPresenter::TryDecomposeInIngredientArea(
	const FVector2D& ScreenPosition,
	ASMergeBoard* Board)
{
	if (!Board || !Board->IsDragging() || !IsInIngredientDropZone(ScreenPosition))
	{
		return false;
	}
	return Board->TryDecomposePieceToInventory(Board->GetActiveDragCellIndex());
}
#pragma endregion K2 moonyfli

bool ASDayBoardPresenter::GetPointerState(FVector2D& OutScreenPosition) const
{
	const APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		return false;
	}

	float TouchX = 0.0f;
	float TouchY = 0.0f;
	bool bTouchPressed = false;
	PlayerController->GetInputTouchState(ETouchIndex::Touch1, TouchX, TouchY, bTouchPressed);
	if (bTouchPressed)
	{
		OutScreenPosition = FVector2D(TouchX, TouchY);
		return true;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	PlayerController->GetMousePosition(MouseX, MouseY);
	OutScreenPosition = FVector2D(MouseX, MouseY);
	return PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
}

AActor* ASDayBoardPresenter::HitTest(const FVector2D& ScreenPosition) const
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		return nullptr;
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SDayBoardPointer), false);
	if (PlayerController->GetHitResultAtScreenPosition(ScreenPosition, ECC_Visibility, Params, Hit))
	{
		return Hit.GetActor();
	}
	return nullptr;
}

void ASDayBoardPresenter::HandlePointerPressed(const FVector2D& ScreenPosition)
{
	ASMergeBoard* Board = LogicBoard.Get();
	if (!Board)
	{
		Board = ASMergeBoard::FindBoard(this);
		LogicBoard = Board;
	}
	if (!Board)
	{
		return;
	}
	bDropHandledOnPress = false;

	if (TryDecomposeInIngredientArea(ScreenPosition, Board))
	{
		bDropHandledOnPress = true;
		RefreshFromLogic();
		return;
	}

	if (ASDayCharacterStandIn* Character = Cast<ASDayCharacterStandIn>(HitTest(ScreenPosition)))
	{
		// Second click of the click-release-click flow: a selected piece lands on the seat.
		if (TryDeliverToCharacter(Character, Board))
		{
			bDropHandledOnPress = true;
		}
		RefreshFromLogic();
		return;
	}

	if (ASDayIngredientBinVisual* Bin = Cast<ASDayIngredientBinVisual>(HitTest(ScreenPosition)))
	{
		Board->TrySpawnFromMotherPiece(Bin->IngredientId);
		RefreshFromLogic();
		return;
	}

	if (ASDayCellVisual* Cell = Cast<ASDayCellVisual>(HitTest(ScreenPosition)))
	{
		if (Board->IsDragging())
		{
			const int32 FromIndex = Board->GetActiveDragCellIndex();
			if (Cell->CellIndex != FromIndex)
			{
				// Second click completes the click-release-click interaction immediately.
				Board->TryDropPiece(FromIndex, Cell->CellIndex);
				bDropHandledOnPress = true;
				RefreshFromLogic();
			}
			return;
		}

		FSDishPiece Piece;
		if (Board->TryGetPiece(Cell->CellIndex, Piece))
		{
			Board->BeginPieceDrag(Cell->CellIndex, 0);
			RefreshFromLogic();
		}
	}
}

void ASDayBoardPresenter::HandlePointerReleased(const FVector2D& ScreenPosition)
{
	ASMergeBoard* Board = LogicBoard.Get();
	if (!Board)
	{
		Board = ASMergeBoard::FindBoard(this);
		LogicBoard = Board;
	}
	if (!Board || !Board->IsDragging())
	{
		return;
	}
	if (bDropHandledOnPress)
	{
		bDropHandledOnPress = false;
		return;
	}

	const int32 FromIndex = Board->GetActiveDragCellIndex();
	if (TryDecomposeInIngredientArea(ScreenPosition, Board))
	{
		RefreshFromLogic();
		return;
	}
	AActor* HitActor = HitTest(ScreenPosition);
	if (ASDayCharacterStandIn* Character = Cast<ASDayCharacterStandIn>(HitActor))
	{
		// Dragging a piece onto a seat delivers it, same as the HUD buttons.
		TryDeliverToCharacter(Character, Board);
		RefreshFromLogic();
		return;
	}

	if (ASDayCellVisual* Cell = Cast<ASDayCellVisual>(HitActor))
	{
		if (Cell->CellIndex != FromIndex)
		{
			// Holding on A and releasing over B completes the drag interaction.
			Board->TryDropPiece(FromIndex, Cell->CellIndex);
		}
	}
	// Releasing a simple click over A (or outside the board) keeps A selected,
	// allowing the next click on B to move or merge it.
	RefreshFromLogic();
}

TSharedRef<SWidget> USDayHUD::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void USDayHUD::NativeConstruct()
{
	Super::NativeConstruct();
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->OnSandboxStateChanged.AddUniqueDynamic(this, &USDayHUD::Refresh);
	}
#pragma region K2 moonyfli
	// The shop clock only advances on tick, so poll it instead of waiting for a state event.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimerHandle, this, &USDayHUD::Refresh, 0.2f, true);
	}
#pragma endregion K2 moonyfli
	Refresh();
}

void USDayHUD::NativeDestruct()
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->OnSandboxStateChanged.RemoveDynamic(this, &USDayHUD::Refresh);
	}
	if (UWorld* World = GetWorld()) //add by K2
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}
	Super::NativeDestruct();
}

void USDayHUD::BuildWidgetTree()
{
	USafeZone* SafeZone = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("DayHUDSafeZone"));
	SafeZone->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = SafeZone;

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DayHUDRoot"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SafeZone->AddChild(Root);

	auto MakeText = [this](const TCHAR* Name, const int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
		return Text;
	};
	auto MakeButton = [this, &MakeText](const TCHAR* Name, const TCHAR* Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = MakeText(*FString::Printf(TEXT("%s_Label"), Name), 22, FLinearColor::White);
		Text->SetText(FText::FromString(Label));
		Text->SetJustification(ETextJustify::Center);
		Button->AddChild(Text);
		return Button;
	};

	PhaseText = MakeText(TEXT("PhaseText"), 24, FLinearColor(0.10f, 0.95f, 0.75f));
	Root->AddChildToVerticalBox(PhaseText)->SetPadding(FMargin(18.0f, 12.0f, 18.0f, 4.0f));

	OrderText = MakeText(TEXT("DayOrderText"), 20, FLinearColor(0.98f, 0.90f, 0.42f));
	Root->AddChildToVerticalBox(OrderText)->SetPadding(FMargin(18.0f, 2.0f, 18.0f, 4.0f));

	UHorizontalBox* Orders = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("OrderRow"));
	Root->AddChildToVerticalBox(Orders)->SetPadding(FMargin(12.0f, 4.0f));
	CustomerButton = MakeButton(TEXT("DayCustomerButton"), TEXT("普通顾客"));
	ALingButton = MakeButton(TEXT("DayALingButton"), TEXT("阿翎"));
	SangPoButton = MakeButton(TEXT("DaySangPoButton"), TEXT("桑婆"));
	Orders->AddChildToHorizontalBox(CustomerButton)->SetPadding(FMargin(4.0f));
	Orders->AddChildToHorizontalBox(ALingButton)->SetPadding(FMargin(4.0f));
	Orders->AddChildToHorizontalBox(SangPoButton)->SetPadding(FMargin(4.0f));
	CustomerButton->OnClicked.AddDynamic(this, &USDayHUD::HandleCustomer);
	ALingButton->OnClicked.AddDynamic(this, &USDayHUD::HandleALing);
	SangPoButton->OnClicked.AddDynamic(this, &USDayHUD::HandleSangPo);

	USizeBox* WorldSpacer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("WorldBoardInputSpace"));
	WorldSpacer->SetHeightOverride(680.0f);
	WorldSpacer->SetVisibility(ESlateVisibility::HitTestInvisible);
	UVerticalBoxSlot* SpacerSlot = Root->AddChildToVerticalBox(WorldSpacer);
	SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	FeedbackText = MakeText(TEXT("DayFeedbackText"), 20, FLinearColor(1.0f, 0.82f, 0.20f));
	Root->AddChildToVerticalBox(FeedbackText)->SetPadding(FMargin(18.0f, 4.0f));

	InventoryText = MakeText(TEXT("DayInventoryText"), 20, FLinearColor::White);
	Root->AddChildToVerticalBox(InventoryText)->SetPadding(FMargin(18.0f, 4.0f));

	UWrapBox* Ingredients = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("IngredientRow"));
	Ingredients->SetInnerSlotPadding(FVector2D(4.0f, 4.0f));
	Root->AddChildToVerticalBox(Ingredients)->SetPadding(FMargin(12.0f, 2.0f));
	UButton* LingGu = MakeButton(TEXT("DayLingGuButton"), TEXT("灵谷"));
	UButton* Yin = MakeButton(TEXT("DayYinShanJunButton"), TEXT("阴山菌"));
	UButton* Chi = MakeButton(TEXT("DayChiYanJiaoButton"), TEXT("赤焰椒"));
	UButton* Yue = MakeButton(TEXT("DayYueLinYuButton"), TEXT("月鳞鱼"));
	UButton* Xuan = MakeButton(TEXT("DayXuanYuQinButton"), TEXT("玄羽禽"));
	Ingredients->AddChildToWrapBox(LingGu);
	Ingredients->AddChildToWrapBox(Yin);
	Ingredients->AddChildToWrapBox(Chi);
	Ingredients->AddChildToWrapBox(Yue);
	Ingredients->AddChildToWrapBox(Xuan);
	LingGu->OnClicked.AddDynamic(this, &USDayHUD::HandleLingGu);
	Yin->OnClicked.AddDynamic(this, &USDayHUD::HandleYinShanJun);
	Chi->OnClicked.AddDynamic(this, &USDayHUD::HandleChiYanJiao);
	Yue->OnClicked.AddDynamic(this, &USDayHUD::HandleYueLinYu);
	Xuan->OnClicked.AddDynamic(this, &USDayHUD::HandleXuanYuQin);

	GiftTabText = MakeText(TEXT("DayGiftTabText"), 18, FLinearColor(0.72f, 0.88f, 1.0f));
	Root->AddChildToVerticalBox(GiftTabText)->SetPadding(FMargin(18.0f, 4.0f));

	UHorizontalBox* Flow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FlowRow"));
	Root->AddChildToVerticalBox(Flow)->SetPadding(FMargin(12.0f, 2.0f, 12.0f, 10.0f));
	FlowButton = MakeButton(TEXT("DayFlowButton"), TEXT("入夜"));
	Flow->AddChildToHorizontalBox(FlowButton)->SetPadding(FMargin(4.0f));
	FlowButton->OnClicked.AddDynamic(this, &USDayHUD::HandleFlowButton);
#pragma region K2 moonyfli
#if !UE_BUILD_SHIPPING
	CheatToggleButton = MakeButton(TEXT("DayCheatToggleButton"), TEXT("修改器"));
	Flow->AddChildToHorizontalBox(CheatToggleButton)->SetPadding(FMargin(4.0f));
	CheatToggleButton->OnClicked.AddDynamic(this, &USDayHUD::HandleToggleCheatPanel);
#endif
#pragma endregion K2 moonyfli
}

void USDayHUD::Refresh()
{
	USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>();
	if (!GameInstance)
	{
		return;
	}

	if (PhaseText)
	{
		const FString ClockLine = GameInstance->IsShopOpen()
			? FString::Printf(TEXT("  剩余 %.0fs"), GameInstance->GetDayTimeRemaining())
			: FString();
		PhaseText->SetText(FText::FromString(FString::Printf(
			TEXT("%s  %s  营业额 %d/%d  缺口 %d%s\n%s"),
			*GameInstance->GetPhaseDisplayName(),
			*GameInstance->StageId.ToString(),
			GameInstance->Revenue,
			GameInstance->RevenueTarget,
			GameInstance->GetRevenueGap(),
			*ClockLine,
			*GameInstance->GetPlannedOrderSummary())));
	}
	if (InventoryText)
	{
		InventoryText->SetText(FText::FromString(FString::Printf(
			TEXT("库存  灵谷 %d｜阴山菌 %d｜赤焰椒 %d｜月鳞鱼 %d｜玄羽禽 %d"),
			GameInstance->GetQuantity(DayLingGuId),
			GameInstance->GetQuantity(DayYinShanJunId),
			GameInstance->GetQuantity(DayChiYanJiaoId),
			GameInstance->GetQuantity(DayYueLinYuId),
			GameInstance->GetQuantity(DayXuanYuQinId))));
	}
	if (OrderText)
	{
		FString OrderLine = TEXT("当前顾客：等待入座");
		FString CustomerButtonText = TEXT("顾客：等待");
		if (const ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
		{
			if (Director->HasActiveCustomer())
			{
				TArray<FString> GuestParts;
				for (const FSCustomerState& Customer : Director->GetActiveCustomers())
				{
					const FString Name = Customer.DisplayName.IsEmpty() ? Customer.CustomerId : Customer.DisplayName;
					GuestParts.Add(FString::Printf(
						TEXT("座%d %s：%s Lv%d"),
						Customer.SeatIndex + 1,
						*Name,
						*IngredientShortName(this, Customer.Order.IngredientId),
						Customer.Order.Level));
				}
				CustomerButtonText = TEXT("交付匹配顾客");
				OrderLine = FString::Printf(
					TEXT("普通顾客（各座独立补客、无限等待）：%s"),
					*FString::Join(GuestParts, TEXT("　｜　")));
			}
			else
			{
				OrderLine = FString::Printf(TEXT("当前顾客：等待入座｜下一位约 %.1fs"), Director->GetSpawnCooldownRemaining());
			}
		}
		if (const ASSpecialNpcDirector* NpcDirector = ASSpecialNpcDirector::FindDirector(this))
		{
			TArray<FString> NpcParts;
			for (const FSSpecialNpcState& Npc : NpcDirector->GetNpcs())
			{
				NpcParts.Add(Npc.bServed
					? FString::Printf(TEXT("%s 已服务✓"), *Npc.DisplayName)
					: FString::Printf(
						TEXT("%s 要 %s Lv%d → %s"),
						*Npc.DisplayName,
						*IngredientShortName(this, Npc.Order.IngredientId),
						Npc.Order.Level,
						*USChefGameInstance::GetGiftDisplayName(Npc.GiftId)));
			}
			if (NpcParts.Num() > 0)
			{
				OrderLine += FString::Printf(TEXT("\n特殊委托：%s"), *FString::Join(NpcParts, TEXT("　｜　")));
			}
		}
		OrderText->SetText(FText::FromString(OrderLine));
		SetButtonText(CustomerButton, CustomerButtonText);
	}
	if (FeedbackText)
	{
		FeedbackText->SetText(FText::FromString(FString::Printf(TEXT("操作：%s"), *GameInstance->LastBoardFeedback)));
	}

	if (GiftTabText)
	{
		// 页签只做展示：谢礼完成订单即得即用，没有勾选也没有背包。
		const bool bBeforeNight = GameInstance->Phase == ESGamePhase::PrepareNight;
		GiftTabText->SetText(FText::FromString(bBeforeNight
			? FString::Printf(TEXT("入夜前｜%s"), *GameInstance->GetGiftTabSummary())
			: GameInstance->GetGiftTabSummary()));
		GiftTabText->SetColorAndOpacity(FSlateColor(bBeforeNight
			? FLinearColor(1.0f, 0.88f, 0.45f)
			: FLinearColor(0.72f, 0.88f, 1.0f)));
	}

	FString FlowLabel;
	switch (GameInstance->Phase)
	{
	case ESGamePhase::Boot:
	case ESGamePhase::PrepareNight: FlowLabel = TEXT("入夜"); break;
	case ESGamePhase::NightRunning: FlowLabel = TEXT("模拟夜间到达终点"); break;
	case ESGamePhase::DayRunning: FlowLabel = TEXT("闭店（未达标将回档）"); break;
	case ESGamePhase::DayQualified: FlowLabel = TEXT("闭店日结"); break;
	case ESGamePhase::Ending: FlowLabel = TEXT("尾声"); break;
	default: break;
	}
	if (FlowButton)
	{
		FlowButton->SetVisibility(FlowLabel.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		SetButtonText(FlowButton, FlowLabel);
	}
}

void USDayHUD::HandleCustomer()
{
	if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
	{
		Director->TryDeliverSelectedPiece();
	}
}

void USDayHUD::DeliverNpc(const FName NpcId)
{
	if (ASSpecialNpcDirector* Director = ASSpecialNpcDirector::FindDirector(this))
	{
		Director->TryDeliverToNpc(NpcId);
	}
}

void USDayHUD::HandleALing() { DeliverNpc(DayNpcALingId); }
void USDayHUD::HandleSangPo() { DeliverNpc(DayNpcSangPoId); }

void USDayHUD::SpawnIngredient(const FName IngredientId)
{
	if (ASMergeBoard* Board = ASMergeBoard::FindBoard(this))
	{
		Board->TrySpawnFromMotherPiece(IngredientId);
	}
}

void USDayHUD::HandleLingGu() { SpawnIngredient(DayLingGuId); }
void USDayHUD::HandleYinShanJun() { SpawnIngredient(DayYinShanJunId); }
void USDayHUD::HandleChiYanJiao() { SpawnIngredient(DayChiYanJiaoId); }
void USDayHUD::HandleYueLinYu() { SpawnIngredient(DayYueLinYuId); }
void USDayHUD::HandleXuanYuQin() { SpawnIngredient(DayXuanYuQinId); }

void USDayHUD::HandleFlowButton()
{
	if (ASFakeNightGateway* Gateway = Cast<ASFakeNightGateway>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ASFakeNightGateway::StaticClass())))
	{
		Gateway->AdvanceFlow();
	}
}

#pragma region K2 moonyfli
void USDayHUD::HandleToggleCheatPanel()
{
#if UE_BUILD_SHIPPING
	return;
#else
	if (!CheatPanel)
	{
		CheatPanel = CreateWidget<USDayCheatPanel>(this, USDayCheatPanel::StaticClass());
		if (CheatPanel)
		{
			// Full-screen overlay; the movable frame is a canvas child inside it.
			CheatPanel->AddToViewport(120);
			CheatPanel->SetPanelVisible(true);
			return;
		}
	}
	if (CheatPanel)
	{
		const bool bShow = CheatPanel->GetVisibility() == ESlateVisibility::Collapsed;
		CheatPanel->SetPanelVisible(bShow);
	}
#endif
}

void USDayCheatPanel::SetPanelVisible(const bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bVisible)
	{
		if (FrameSlot)
		{
			FrameSlot->SetPosition(FramePosition);
		}
		RefreshStatus();
	}
}

TSharedRef<SWidget> USDayCheatPanel::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
	return Super::RebuildWidget();
}

void USDayCheatPanel::NativeConstruct()
{
	Super::NativeConstruct();
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->OnSandboxStateChanged.AddUniqueDynamic(this, &USDayCheatPanel::RefreshStatus);
	}
	RefreshStatus();
}

void USDayCheatPanel::NativeDestruct()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->OnSandboxStateChanged.RemoveDynamic(this, &USDayCheatPanel::RefreshStatus);
	}
	Super::NativeDestruct();
}

void USDayCheatPanel::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const FVector2D CanvasSize = FVector2D(MyGeometry.GetLocalSize());
	if (!FrameSizeBox || CanvasSize.X < 1.0 || CanvasSize.Y < 1.0)
	{
		return;
	}

	// Portrait phone layouts leave little room, so size the frame from the space actually available.
	const float TargetWidth = FMath::Clamp(static_cast<float>(CanvasSize.X) * 0.62f, 300.0f, 560.0f);
	const float TargetHeight = FMath::Clamp(static_cast<float>(CanvasSize.Y - FramePosition.Y) - 80.0f, 120.0f, 560.0f);
	if (!FMath::IsNearlyEqual(AppliedFrameWidth, TargetWidth, 1.0f))
	{
		AppliedFrameWidth = TargetWidth;
		FrameSizeBox->SetWidthOverride(TargetWidth);
	}
	if (!FMath::IsNearlyEqual(AppliedFrameHeight, TargetHeight, 1.0f))
	{
		AppliedFrameHeight = TargetHeight;
		ApplyBodyHeight();
	}

	if (!bDraggingPanel)
	{
		ApplyFramePosition(FramePosition, MyGeometry);
	}
}

FReply USDayCheatPanel::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bLeftButton = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	const bool bOnHandle = DragHandle && DragHandle->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition());
	if (!bLeftButton || !bOnHandle)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	bDraggingPanel = true;
	const FVector2D LocalCursor = FVector2D(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
	DragGrabOffset = LocalCursor - FramePosition;
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply USDayCheatPanel::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDraggingPanel)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	const FVector2D LocalCursor = FVector2D(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
	ApplyFramePosition(LocalCursor - DragGrabOffset, InGeometry);
	return FReply::Handled();
}

FReply USDayCheatPanel::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDraggingPanel)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	bDraggingPanel = false;
	return FReply::Handled().ReleaseMouseCapture();
}

void USDayCheatPanel::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	bDraggingPanel = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void USDayCheatPanel::ApplyFramePosition(const FVector2D& DesiredPosition, const FGeometry& CanvasGeometry)
{
	const FVector2D CanvasSize = FVector2D(CanvasGeometry.GetLocalSize());
	const FVector2D FrameSize = Frame ? FVector2D(Frame->GetCachedGeometry().GetLocalSize()) : FVector2D::ZeroVector;

	FVector2D Clamped = DesiredPosition;
	if (CanvasSize.X > 1.0 && CanvasSize.Y > 1.0)
	{
		// Keep a sliver of the drag handle on screen so the panel stays grabbable.
		const double MinX = 80.0 - FMath::Max(FrameSize.X, 80.0);
		Clamped.X = FMath::Clamp(Clamped.X, MinX, FMath::Max(MinX, CanvasSize.X - 80.0));
		Clamped.Y = FMath::Clamp(Clamped.Y, 0.0, FMath::Max(0.0, CanvasSize.Y - 48.0));
	}

	if (FrameSlot && !Clamped.Equals(FramePosition, 0.5))
	{
		FrameSlot->SetPosition(Clamped);
	}
	FramePosition = Clamped;
}

void USDayCheatPanel::HandleToggleBody()
{
	if (!BodySizeBox)
	{
		return;
	}
	const bool bCollapse = BodySizeBox->GetVisibility() != ESlateVisibility::Collapsed;
	BodySizeBox->SetVisibility(bCollapse ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (CollapseLabel)
	{
		CollapseLabel->SetText(FText::FromString(bCollapse ? TEXT("展开") : TEXT("收起")));
	}
}

void USDayCheatPanel::ApplyBodyHeight()
{
	if (BodySizeBox && AppliedFrameHeight > 0.0f)
	{
		BodySizeBox->SetHeightOverride(AppliedFrameHeight);
	}
}

USChefGameInstance* USDayCheatPanel::GetChef() const
{
	return GetGameInstance<USChefGameInstance>();
}

void USDayCheatPanel::BuildWidgetTree()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CheatCanvas"));
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CheatFrame"));
	Frame->SetBrushColor(FLinearColor(0.05f, 0.07f, 0.10f, 0.92f));
	Frame->SetPadding(FMargin(10.0f));
	FrameSlot = Canvas->AddChildToCanvas(Frame);
	FrameSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	FrameSlot->SetAlignment(FVector2D::ZeroVector);
	FrameSlot->SetAutoSize(true);
	FrameSlot->SetPosition(FramePosition);

	FrameSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CheatSize"));
	FrameSizeBox->SetWidthOverride(520.0f);
	Frame->AddChild(FrameSizeBox);

	UVerticalBox* Outer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CheatOuter"));
	FrameSizeBox->AddChild(Outer);

	BodySizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CheatBodySize"));
	BodyScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CheatScroll"));
	BodySizeBox->AddChild(BodyScroll);

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CheatRoot"));
	BodyScroll->AddChild(Root);

	auto MakeText = [this](const TCHAR* Name, const int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
		return Text;
	};
	auto MakeButton = [this, &MakeText](const TCHAR* Name, const TCHAR* Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UTextBlock* Text = MakeText(*FString::Printf(TEXT("%s_Label"), Name), 16, FLinearColor::White);
		Text->SetText(FText::FromString(Label));
		Text->SetJustification(ETextJustify::Center);
		// Wrapping would let the wrap box squeeze labels into unreadable stacks.
		Text->SetAutoWrapText(false);
		Button->AddChild(Text);
		return Button;
	};
	auto AddSection = [&](const TCHAR* Title)
	{
		UTextBlock* TitleText = MakeText(*FString::Printf(TEXT("Sec_%s"), Title), 18, FLinearColor(0.55f, 0.95f, 0.80f));
		TitleText->SetText(FText::FromString(Title));
		Root->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(4.0f, 8.0f, 4.0f, 2.0f));
	};
	auto AddRow = [&]() -> UWrapBox*
	{
		UWrapBox* Row = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
		Row->SetInnerSlotPadding(FVector2D(4.0f, 4.0f));
		Root->AddChildToVerticalBox(Row)->SetPadding(FMargin(2.0f));
		return Row;
	};

	DragHandle = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CheatDragHandle"));
	DragHandle->SetBrushColor(FLinearColor(0.12f, 0.16f, 0.22f, 0.95f));
	DragHandle->SetPadding(FMargin(6.0f, 4.0f));
	DragHandle->SetVisibility(ESlateVisibility::Visible);
	Outer->AddChildToVerticalBox(DragHandle);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CheatHeader"));
	DragHandle->AddChild(Header);
	UTextBlock* Title = MakeText(TEXT("CheatTitle"), 22, FLinearColor(1.0f, 0.85f, 0.35f));
	Title->SetText(FText::FromString(TEXT("白天修改器（拖动此标题栏）")));
	Header->AddChildToHorizontalBox(Title)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UButton* CollapseButton = MakeButton(TEXT("CheatCollapse"), TEXT("收起"));
	CollapseLabel = Cast<UTextBlock>(CollapseButton->GetChildAt(0));
	Header->AddChildToHorizontalBox(CollapseButton)->SetPadding(FMargin(4.0f, 0.0f));
	CollapseButton->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleToggleBody);
	UButton* CloseButton = MakeButton(TEXT("CheatClose"), TEXT("关闭"));
	Header->AddChildToHorizontalBox(CloseButton);
	CloseButton->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleClose);

	Outer->AddChildToVerticalBox(BodySizeBox);

	StatusText = MakeText(TEXT("CheatStatus"), 16, FLinearColor(0.90f, 0.90f, 0.90f));
	Root->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(4.0f, 4.0f));

	AddSection(TEXT("食材（先选目标）"));
	{
		UWrapBox* Row = AddRow();
		UButton* B0 = MakeButton(TEXT("CheatSelLing"), TEXT("灵谷"));
		UButton* B1 = MakeButton(TEXT("CheatSelYin"), TEXT("阴山菌"));
		UButton* B2 = MakeButton(TEXT("CheatSelChi"), TEXT("赤焰椒"));
		UButton* B3 = MakeButton(TEXT("CheatSelYue"), TEXT("月鳞鱼"));
		UButton* B4 = MakeButton(TEXT("CheatSelXuan"), TEXT("玄羽禽"));
		Row->AddChildToWrapBox(B0);
		Row->AddChildToWrapBox(B1);
		Row->AddChildToWrapBox(B2);
		Row->AddChildToWrapBox(B3);
		Row->AddChildToWrapBox(B4);
		B0->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleSelectLingGu);
		B1->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleSelectYin);
		B2->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleSelectChi);
		B3->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleSelectYue);
		B4->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleSelectXuan);
	}
	{
		UWrapBox* Row = AddRow();
		UButton* P1 = MakeButton(TEXT("CheatPlus1"), TEXT("+1"));
		UButton* P10 = MakeButton(TEXT("CheatPlus10"), TEXT("+10"));
		UButton* Clr = MakeButton(TEXT("CheatClear"), TEXT("清零"));
		Row->AddChildToWrapBox(P1);
		Row->AddChildToWrapBox(P10);
		Row->AddChildToWrapBox(Clr);
		P1->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleStockPlus1);
		P10->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleStockPlus10);
		Clr->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleStockClear);
	}
	{
		UWrapBox* Row = AddRow();
		UButton* S0 = MakeButton(TEXT("CheatSet0"), TEXT("设0"));
		UButton* S10 = MakeButton(TEXT("CheatSet10"), TEXT("设10"));
		UButton* S20 = MakeButton(TEXT("CheatSet20"), TEXT("设20"));
		UButton* S50 = MakeButton(TEXT("CheatSet50"), TEXT("设50"));
		UButton* S99 = MakeButton(TEXT("CheatSet99"), TEXT("设99"));
		Row->AddChildToWrapBox(S0);
		Row->AddChildToWrapBox(S10);
		Row->AddChildToWrapBox(S20);
		Row->AddChildToWrapBox(S50);
		Row->AddChildToWrapBox(S99);
		S0->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleStockSet0);
		S10->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleStockSet10);
		S20->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleStockSet20);
		S50->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleStockSet50);
		S99->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleStockSet99);
	}

	AddSection(TEXT("客人"));
	{
		UWrapBox* Row = AddRow();
		UButton* Next = MakeButton(TEXT("CheatNextGuest"), TEXT("下一位立刻到"));
		Row->AddChildToWrapBox(Next);
		Next->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleForceNextCustomer);
	}

	AddSection(TEXT("营业额 / 时间"));
	{
		UWrapBox* Row = AddRow();
		UButton* R10 = MakeButton(TEXT("CheatRev10"), TEXT("额+10"));
		UButton* R50 = MakeButton(TEXT("CheatRev50"), TEXT("额+50"));
		UButton* RQ = MakeButton(TEXT("CheatRevQ"), TEXT("直接达标"));
		Row->AddChildToWrapBox(R10);
		Row->AddChildToWrapBox(R50);
		Row->AddChildToWrapBox(RQ);
		R10->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleRevenuePlus10);
		R50->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleRevenuePlus50);
		RQ->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleRevenueQualify);
	}
	{
		UWrapBox* Row = AddRow();
		UButton* T30 = MakeButton(TEXT("CheatTimeP30"), TEXT("时+30"));
		UButton* Tm30 = MakeButton(TEXT("CheatTimeM30"), TEXT("时-30"));
		UButton* T60 = MakeButton(TEXT("CheatTime60"), TEXT("时=60"));
		UButton* T10 = MakeButton(TEXT("CheatTime10"), TEXT("时=10"));
		Row->AddChildToWrapBox(T30);
		Row->AddChildToWrapBox(Tm30);
		Row->AddChildToWrapBox(T60);
		Row->AddChildToWrapBox(T10);
		T30->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleTimePlus30);
		Tm30->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleTimeMinus30);
		T60->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleTimeSet60);
		T10->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleTimeSet10);
	}

	AddSection(TEXT("流程"));
	{
		UWrapBox* Row = AddRow();
		UButton* Open = MakeButton(TEXT("CheatOpen"), TEXT("强制开店"));
		UButton* Close = MakeButton(TEXT("CheatCloseShop"), TEXT("强制闭店"));
		UButton* Fail = MakeButton(TEXT("CheatFailDay"), TEXT("日失败回档"));
		Row->AddChildToWrapBox(Open);
		Row->AddChildToWrapBox(Close);
		Row->AddChildToWrapBox(Fail);
		Open->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleOpenShop);
		Close->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleForceClose);
		Fail->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleFailDay);
	}

	AddSection(TEXT("谢礼"));
	{
		UWrapBox* Row = AddRow();
		UButton* G0 = MakeButton(TEXT("CheatGiftKite"), TEXT("纸鸢"));
		UButton* G1 = MakeButton(TEXT("CheatGiftLamp"), TEXT("纸灯"));
		UButton* G2 = MakeButton(TEXT("CheatGiftCoin"), TEXT("铜钱"));
		UButton* G3 = MakeButton(TEXT("CheatGiftBox"), TEXT("食盒"));
		UButton* GC = MakeButton(TEXT("CheatGiftClear"), TEXT("清空谢礼"));
		Row->AddChildToWrapBox(G0);
		Row->AddChildToWrapBox(G1);
		Row->AddChildToWrapBox(G2);
		Row->AddChildToWrapBox(G3);
		Row->AddChildToWrapBox(GC);
		G0->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleGiftKite);
		G1->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleGiftLamp);
		G2->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleGiftCoin);
		G3->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleGiftBox);
		GC->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleClearGifts);
	}

	AddSection(TEXT("跳关"));
	{
		UWrapBox* Row = AddRow();
		UButton* J0 = MakeButton(TEXT("CheatJumpT0"), TEXT("T0"));
		UButton* J1 = MakeButton(TEXT("CheatJumpL1"), TEXT("L1"));
		UButton* J2 = MakeButton(TEXT("CheatJumpL2"), TEXT("L2"));
		UButton* J3 = MakeButton(TEXT("CheatJumpL3"), TEXT("L3"));
		Row->AddChildToWrapBox(J0);
		Row->AddChildToWrapBox(J1);
		Row->AddChildToWrapBox(J2);
		Row->AddChildToWrapBox(J3);
		J0->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleJumpT0);
		J1->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleJumpL1);
		J2->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleJumpL2);
		J3->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleJumpL3);
	}

	AddSection(TEXT("订单队列"));
	OrderQueueText = MakeText(TEXT("CheatOrderQueue"), 15, FLinearColor(0.95f, 0.88f, 0.55f));
	Root->AddChildToVerticalBox(OrderQueueText)->SetPadding(FMargin(4.0f, 2.0f));

	AddSection(TEXT("存档"));
	{
		UWrapBox* Row = AddRow();
		UButton* Save = MakeButton(TEXT("CheatSave"), TEXT("存档"));
		UButton* Load = MakeButton(TEXT("CheatLoad"), TEXT("读档"));
		UButton* Del = MakeButton(TEXT("CheatDel"), TEXT("删档"));
		UButton* Bad = MakeButton(TEXT("CheatCorrupt"), TEXT("坏档"));
		Row->AddChildToWrapBox(Save);
		Row->AddChildToWrapBox(Load);
		Row->AddChildToWrapBox(Del);
		Row->AddChildToWrapBox(Bad);
		Save->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleSave);
		Load->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleLoad);
		Del->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleDeleteSave);
		Bad->OnClicked.AddDynamic(this, &USDayCheatPanel::HandleCorruptSave);
	}
}

void USDayCheatPanel::RefreshStatus()
{
	USChefGameInstance* GameInstance = GetChef();
	if (!GameInstance)
	{
		return;
	}
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(FString::Printf(
			TEXT("%s | %s | 额 %d/%d | 时 %.1fs | 选中 %s=%d\n%s\n%s"),
			*GameInstance->GetPhaseDisplayName(),
			*GameInstance->StageId.ToString(),
			GameInstance->Revenue,
			GameInstance->RevenueTarget,
			GameInstance->GetDayTimeRemaining(),
			*GameInstance->ResolveIngredientDisplayName(SelectedIngredientId),
			GameInstance->GetQuantity(SelectedIngredientId),
			*GameInstance->LastBoardFeedback,
			*GameInstance->LastSaveFeedback)));
	}
	if (OrderQueueText)
	{
		OrderQueueText->SetText(FText::FromString(GameInstance->GetPlannedOrderSummary()));
	}
}

void USDayCheatPanel::SetSelectedIngredient(const FName IngredientId)
{
	SelectedIngredientId = IngredientId;
	RefreshStatus();
}

void USDayCheatPanel::AdjustStock(const int32 Delta)
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		if (Delta >= 0)
		{
			GameInstance->GrantPermanentStock(SelectedIngredientId, Delta);
		}
		else
		{
			GameInstance->SetInventoryQuantityForDebug(
				SelectedIngredientId,
				FMath::Max(0, GameInstance->GetQuantity(SelectedIngredientId) + Delta));
		}
	}
}

void USDayCheatPanel::SetStock(const int32 Quantity)
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->SetInventoryQuantityForDebug(SelectedIngredientId, Quantity);
	}
}

void USDayCheatPanel::HandleClose() { SetPanelVisible(false); }
void USDayCheatPanel::HandleSelectLingGu() { SetSelectedIngredient(DayLingGuId); }
void USDayCheatPanel::HandleSelectYin() { SetSelectedIngredient(DayYinShanJunId); }
void USDayCheatPanel::HandleSelectChi() { SetSelectedIngredient(DayChiYanJiaoId); }
void USDayCheatPanel::HandleSelectYue() { SetSelectedIngredient(DayYueLinYuId); }
void USDayCheatPanel::HandleSelectXuan() { SetSelectedIngredient(DayXuanYuQinId); }
void USDayCheatPanel::HandleStockPlus1() { AdjustStock(1); }
void USDayCheatPanel::HandleStockPlus10() { AdjustStock(10); }
void USDayCheatPanel::HandleStockClear() { SetStock(0); }
void USDayCheatPanel::HandleStockSet0() { SetStock(0); }
void USDayCheatPanel::HandleStockSet10() { SetStock(10); }
void USDayCheatPanel::HandleStockSet20() { SetStock(20); }
void USDayCheatPanel::HandleStockSet50() { SetStock(50); }
void USDayCheatPanel::HandleStockSet99() { SetStock(99); }

void USDayCheatPanel::HandleForceNextCustomer()
{
	if (ASCustomerDirector* Director = ASCustomerDirector::FindDirector(this))
	{
		Director->ForceNextCustomersNow();
	}
}

void USDayCheatPanel::HandleRevenuePlus10()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->AddRevenue(10);
	}
}

void USDayCheatPanel::HandleRevenuePlus50()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->AddRevenue(50);
	}
}

void USDayCheatPanel::HandleRevenueQualify()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->ForceQualifyRevenueForDebug();
	}
}

void USDayCheatPanel::HandleTimePlus30()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->SetDayTimeRemainingForDebug(GameInstance->GetDayTimeRemaining() + 30.0f);
	}
}

void USDayCheatPanel::HandleTimeMinus30()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->SetDayTimeRemainingForDebug(GameInstance->GetDayTimeRemaining() - 30.0f);
	}
}

void USDayCheatPanel::HandleTimeSet60()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->SetDayTimeRemainingForDebug(60.0f);
	}
}

void USDayCheatPanel::HandleTimeSet10()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->SetDayTimeRemainingForDebug(10.0f);
	}
}

void USDayCheatPanel::HandleOpenShop()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->OpenShopForDebug();
	}
}

void USDayCheatPanel::HandleForceClose()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->ForceCloseShopForDebug();
	}
}

void USDayCheatPanel::HandleFailDay()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->FailDayForDebug();
	}
}

void USDayCheatPanel::HandleGiftKite()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->GrantGift(TEXT("GuideKite"));
	}
}

void USDayCheatPanel::HandleGiftLamp()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->GrantGift(TEXT("LifeLamp"));
	}
}

void USDayCheatPanel::HandleGiftCoin()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->GrantGift(TEXT("BeatCoin"));
	}
}

void USDayCheatPanel::HandleGiftBox()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->GrantGift(TEXT("GluttonBox"));
	}
}

void USDayCheatPanel::HandleClearGifts()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->ClearActiveGiftsForDebug();
	}
}

void USDayCheatPanel::HandleJumpT0()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->JumpToStageForDebug(TEXT("T0"));
	}
}

void USDayCheatPanel::HandleJumpL1()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->JumpToStageForDebug(TEXT("L1"));
	}
}

void USDayCheatPanel::HandleJumpL2()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->JumpToStageForDebug(TEXT("L2"));
	}
}

void USDayCheatPanel::HandleJumpL3()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->JumpToStageForDebug(TEXT("L3"));
	}
}

void USDayCheatPanel::HandleSave()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->SaveChefProfile();
	}
}

void USDayCheatPanel::HandleLoad()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->LoadChefProfile();
	}
}

void USDayCheatPanel::HandleDeleteSave()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->DeleteChefProfile();
	}
}

void USDayCheatPanel::HandleCorruptSave()
{
	if (USChefGameInstance* GameInstance = GetChef())
	{
		GameInstance->SimulateCorruptSaveForDebug();
	}
}
#pragma endregion K2 moonyfli
