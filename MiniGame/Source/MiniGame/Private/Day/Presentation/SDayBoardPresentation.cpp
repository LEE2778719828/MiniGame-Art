#include "Day/Presentation/SDayBoardPresentation.h"

#include "../../../SStandaloneSandbox.h"
#include "Day/UI/SRestaurantEndDialogueWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h" //add by K2
#include "Components/Border.h"
#include "Components/BillboardComponent.h"
#include "Components/Button.h"
#include "Components/CheckBox.h" //add by K2
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/SkeletalMeshComponent.h" //add by K2
#include "Components/SkyLightComponent.h"
#include "Components/SizeBox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Curves/CurveFloat.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WidgetComponent.h"
#include "Animation/AnimationAsset.h" //add by K2
#include "Animation/AnimSequenceBase.h" //add by K2
#include "TimerManager.h" //add by K2
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TextureResource.h" //add by K2
#include "Day/Input/SDayPlayerController.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "SceneView.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

// Revenue feedback is viewport-only; the restaurant camera and world composition remain untouched.
namespace DayBoardPresentationPrivate
{
	// The authored box order stays fixed so changing an output never changes a hit zone.
	constexpr int32 DayIngredientBinCount = 5;
	// Keep global identifiers trivially initialized. FName construction must wait until
	// the UObject/name subsystem is ready; otherwise the primary module can stall while
	// the editor is loading its project DLL.
	struct FLazyName
	{
		const TCHAR* Literal = TEXT("");

		FORCEINLINE operator FName() const
		{
			return FName(Literal);
		}
	};

	struct FLazyString
	{
		const TCHAR* Literal = TEXT("");

		FORCEINLINE operator FString() const
		{
			return FString(Literal);
		}
	};

	constexpr FLazyName DayLingGuId{TEXT("LingGu")};
	constexpr FLazyName DayYinShanJunId{TEXT("YinShanJun")};
	constexpr FLazyName DayChiYanJiaoId{TEXT("ChiYanJiao")};
	constexpr FLazyName DayYueLinYuId{TEXT("YueLinYu")};
	constexpr FLazyName DayXuanYuQinId{TEXT("XuanYuQin")};
	constexpr FLazyName DayNpcALingId{TEXT("ALing")};
	constexpr FLazyName DayNpcSangPoId{TEXT("SangPo")};

	UStaticMesh* LoadBasicShape(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

#pragma region K2 moonyfli
	/**
	 * Tag on the camera that frames the board, carried by BP_DayCamera's Camera component.
	 * A tagged camera becomes the view target verbatim, so framing is dialled in the editor
	 * viewport instead of recompiled, and swapping in another camera Blueprint is a matter of
	 * moving the tag (see Tools/SwitchDayStageCamera.py). Exactly one camera may carry it.
	 */
	constexpr FLazyName DayLevelCameraTag{TEXT("SDayCamera")};
	constexpr FLazyName DayArtEnvironmentTag{TEXT("SDay.Environment")};
	constexpr FLazyName DayArtBoardTag{TEXT("SDay.Board")};
	/** Plane that fills the camera frame, so the picture's edge is visible against the letterbox. */
	constexpr FLazyName DayArtBackdropTag{TEXT("SDay.Backdrop")};
	/** Dish plate row along the top of frame; customers belong behind it. */
	constexpr FLazyName DayArtCustomerPlatesTag{TEXT("SDay.CustomerPlates")};
	/** Marks a cell whose origin already sits at the bottom of an art well. */
	constexpr FLazyName DayCellSeatedTag{TEXT("SDay.Cell.Seated")};

	/** Shipping target is a 1440x3200 portrait phone panel. */
	constexpr float DayPortraitAspectRatio = 1440.0f / 3200.0f;

	/**
	 * A tagged piece of dressing, resolved from either an actor tag or a component tag. The
	 * canguan restaurant ships as one Blueprint whose meshes carry the SDay.* tags per component,
	 * while the whitebox and older setups tag standalone actors; the layout code only ever needs
	 * a transform and world bounds, so both spellings answer the same questions.
	 */
	struct FDayArtPiece
	{
		const AActor* Actor = nullptr;
		const UPrimitiveComponent* Component = nullptr;

		bool IsValid() const
		{
			return Component != nullptr || Actor != nullptr;
		}

		FTransform GetTransform() const
		{
			if (Component)
			{
				return Component->GetComponentTransform();
			}
			return Actor ? Actor->GetActorTransform() : FTransform::Identity;
		}

		void GetBounds(FVector& OutCenter, FVector& OutExtent) const
		{
			OutCenter = FVector::ZeroVector;
			OutExtent = FVector::ZeroVector;
			if (Component)
			{
				const FBoxSphereBounds ComponentBounds = Component->Bounds;
				OutCenter = ComponentBounds.Origin;
				OutExtent = ComponentBounds.BoxExtent;
				return;
			}
			if (Actor)
			{
				Actor->GetActorBounds(false, OutCenter, OutExtent);
			}
		}
	};

	FDayArtPiece FindDayArtPiece(UWorld* World, const FName Tag)
	{
		FDayArtPiece Piece;
		if (!World || Tag.IsNone())
		{
			return Piece;
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(Tag))
			{
				Piece.Actor = *It;
				return Piece;
			}
			for (const UActorComponent* Component : It->GetComponents())
			{
				const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
				if (Primitive && Primitive->ComponentHasTag(Tag))
				{
					Piece.Actor = *It;
					Piece.Component = Primitive;
					return Piece;
				}
			}
		}
		return Piece;
	}

	void DisableDayArtCollision(UWorld* World)
	{
		if (!World)
		{
			return;
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			bool bIsDayArt = It->ActorHasTag(DayArtEnvironmentTag);
			for (UActorComponent* Component : It->GetComponents())
			{
				for (const FName Tag : Component->ComponentTags)
				{
					if (Tag.ToString().StartsWith(TEXT("SDay.")))
					{
						bIsDayArt = true;
						break;
					}
				}
				if (bIsDayArt)
				{
					break;
				}
			}
			if (bIsDayArt)
			{
				It->SetActorEnableCollision(false);
			}
		}
	}

	FName DayArtBinTag(const int32 Index)
	{
		return FName(*FString::Printf(TEXT("SDay.Bin.%d"), Index));
	}

	/**
	 * Cell rows are authored in the pan's local space (see Tools/GeneratePanCellLayout.py),
	 * so placing them is a composition with the live pan transform. Scale is dropped: the pan
	 * is scaled up to fill the stall, but the cell proxies carry their own radius.
	 */
	FTransform DayArtCellToWorld(const FTransform& BoardTransform, const FTransform& CellLocal)
	{
		return FTransform(
			BoardTransform.GetRotation() * CellLocal.GetRotation(),
			BoardTransform.TransformPosition(CellLocal.GetLocation()),
			FVector::OneVector);
	}

	/**
	 * Intensity the key light needs once it points along the view axis instead of the oblique
	 * whitebox angle. The project renders at a fixed exposure (r.DefaultFeature.AutoExposure is
	 * off), so this is the frame's only brightness knob, and the restaurant meshes are still flat
	 * grey placeholders: this is what lands them mid-frame instead of near white.
	 */
	constexpr float DayArtKeyLightIntensity = 0.7f;

	/**
	 * Restaurant art is already painted in warm wood tones, so the whitebox's warm key stacked a
	 * second yellow cast on top of it. A near-neutral key lets the albedo carry the warmth.
	 */
	const FLinearColor DayArtKeyLightColor(1.0f, 0.985f, 0.965f);

	/**
	 * Key light shares the camera rotation so the composition is lit back-to-front along the
	 * view axis. The pan is a panel tilted toward the camera, so this is also what makes its
	 * face read as the brightest surface in frame.
	 */
	void AimKeyLightAlongCamera(UDirectionalLightComponent* KeyLight, const FRotator& CameraRotation)
	{
		if (KeyLight)
		{
			KeyLight->SetWorldRotation(CameraRotation);
			KeyLight->SetIntensity(DayArtKeyLightIntensity);
			KeyLight->SetLightColor(DayArtKeyLightColor);
		}
	}

	/**
	 * The composition camera. It ships as a component of the canguan Blueprint, so that the
	 * cookingUI layers can hang off it and follow every re-frame, but a standalone tagged
	 * ACameraActor still answers for the whitebox and for older levels.
	 */
	UCameraComponent* FindDayCameraComponent(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(DayLevelCameraTag))
			{
				if (UCameraComponent* Camera = It->FindComponentByClass<UCameraComponent>())
				{
					return Camera;
				}
			}
			for (UActorComponent* Component : It->GetComponents())
			{
				UCameraComponent* Camera = Cast<UCameraComponent>(Component);
				if (Camera && Camera->ComponentHasTag(DayLevelCameraTag))
				{
					return Camera;
				}
			}
		}
		return nullptr;
	}

	/** Composition frame of the level camera: X is screen right, Y screen up, Z depth. */
	struct FDayCameraFrame
	{
		FVector Origin = FVector::ZeroVector;
		FVector Right = FVector::ZeroVector;
		FVector Up = FVector::ZeroVector;
		FVector Forward = FVector::ZeroVector;

		FVector ToFrame(const FVector& World) const
		{
			const FVector Delta = World - Origin;
			return FVector(Delta | Right, Delta | Up, Delta | Forward);
		}

		FVector ToWorld(const FVector& Frame) const
		{
			return Origin + Right * Frame.X + Up * Frame.Y + Forward * Frame.Z;
		}
	};

	bool TryGetDayCameraFrame(UWorld* World, FDayCameraFrame& OutFrame)
	{
		const UCameraComponent* CameraComponent = FindDayCameraComponent(World);
		if (!CameraComponent)
		{
			return false;
		}

		const FTransform CameraToWorld = CameraComponent->GetComponentTransform();
		OutFrame.Origin = CameraToWorld.GetLocation();
		OutFrame.Right = CameraToWorld.GetUnitAxis(EAxis::Y);
		OutFrame.Up = CameraToWorld.GetUnitAxis(EAxis::Z);
		OutFrame.Forward = CameraToWorld.GetUnitAxis(EAxis::X);
		return true;
	}

	/** Clearance the backdrop keeps behind the furthest piece of dressing. */
	constexpr float DayArtBackdropDepthMargin = 200.0f;

	/** The engine plane mesh is 100 x 100 cm at scale 1. */
	constexpr float DayArtBackdropPlaneSize = 100.0f;

	/** Depth between stacked layers; higher order steps toward the camera in both roles. */
	constexpr float DayArtBackdropLayerStep = 15.0f;

	/** Clearance the foreground overlay keeps in front of the nearest piece of dressing. */
	constexpr float DayArtForegroundDepthMargin = 120.0f;

	/** Never let a foreground plane drift behind the camera's near clip. */
	constexpr float DayArtForegroundMinDepth = 60.0f;

	/** Backdrop layers sit behind the stall; tag "SDay.BackdropOrder.N", 0 = furthest street. */
	constexpr FLazyString DayArtBackdropOrderPrefix{TEXT("SDay.BackdropOrder.")};

	/** Overlay layers sit in front of the stall; tag "SDay.ForegroundOrder.N", higher = nearer. */
	constexpr FLazyName DayArtForegroundTag{TEXT("SDay.Foreground")};
	constexpr FLazyString DayArtForegroundOrderPrefix{TEXT("SDay.ForegroundOrder.")};

	int32 DayArtLayerOrder(const TArray<FName>& Tags, const FString& Prefix)
	{
		for (const FName& Tag : Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(Prefix))
			{
				return FCString::Atoi(*TagString.Mid(Prefix.Len()));
			}
		}
		return 0;
	}

	/**
	 * How wide the camera's frame is at a given distance along the view axis. Orthographic zoom
	 * is distance-independent; a perspective camera widens with distance, and the project
	 * constrains the horizontal FOV (ASPECT_RATIO_MAINTAIN_XFOV), so FieldOfView is horizontal.
	 */
	float DayCameraFrameWidthAtDepth(const UCameraComponent& Camera, const float Depth)
	{
		if (Camera.ProjectionMode == ECameraProjectionMode::Orthographic)
		{
			return Camera.OrthoWidth;
		}
		return 2.0f * Depth * FMath::Tan(FMath::DegreesToRadians(0.5f * Camera.FieldOfView));
	}

	/**
	 * Refit the cookingUI layers inside the camera. The art is authored as full-frame slices: four
	 * backdrops (street -> crowd -> storefront -> interior) behind the 3D stall, and three overlays
	 * (coins, tally, red rope) in front of it. The planes are components of the camera, so panning
	 * or re-rotating the camera carries the whole picture with it and nothing can drift out of
	 * register. Depth and fill can be solved in camera-local space so each layer covers the frame
	 * and the stall stays sandwiched, but that solve must stay opt-in: the editor camera preview
	 * shows the authored component transforms, and PIE has to use those same numbers. Invoking this
	 * from BeginPlay or OnConstruction rewrites the stack and makes the two views disagree.
	 */
	void FitDayArtLayers(UWorld* World)
	{
		UCameraComponent* CameraComponent = FindDayCameraComponent(World);
		FDayCameraFrame Frame;
		if (!CameraComponent || !TryGetDayCameraFrame(World, Frame))
		{
			return;
		}

		const float AspectRatio = CameraComponent->AspectRatio;
		if (AspectRatio <= 0.0f)
		{
			return;
		}

		// Everything hanging off the camera has to be out of the measurement below: the layer planes
		// themselves, but also the camera's own editor proxy mesh and frustum, whose bounds sit at
		// the camera and would peg the near edge of the bracket to nothing.
		auto RidesTheCamera = [CameraComponent](const USceneComponent* Component)
		{
			for (const USceneComponent* Node = Component; Node; Node = Node->GetAttachParent())
			{
				if (Node == CameraComponent)
				{
					return true;
				}
			}
			return false;
		};

		// Bracket the real dressing along the view axis: backdrops go behind the furthest piece,
		// overlays in front of the nearest. Measured per mesh rather than per actor, because the
		// whole restaurant is now one actor whose union bounds would be far looser than the
		// individual pieces.
		float FarDepth = 0.0f;
		float NearDepth = TNumericLimits<float>::Max();
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			// Pre-Blueprint levels dressed the layers as standalone actors that also carry the
			// environment tag; they are art, not dressing to bracket against.
			if (!It->ActorHasTag(DayArtEnvironmentTag)
				|| It->ActorHasTag(DayArtBackdropTag)
				|| It->ActorHasTag(DayArtForegroundTag))
			{
				continue;
			}
			for (const UActorComponent* Component : It->GetComponents())
			{
				const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
				if (!Primitive || RidesTheCamera(Primitive))
				{
					continue;
				}
				const FBoxSphereBounds Bounds = Primitive->Bounds;
				const float Along = Frame.ToFrame(Bounds.Origin).Z;
				const float Radius = Bounds.BoxExtent.Size();
				FarDepth = FMath::Max(FarDepth, Along + Radius);
				NearDepth = FMath::Min(NearDepth, Along - Radius);
			}
		}
		if (NearDepth == TNumericLimits<float>::Max())
		{
			NearDepth = FarDepth;
		}
		FarDepth += DayArtBackdropDepthMargin;

		// The restaurant includes wide floor and wall planes that reach most of the way back to
		// the camera, so the nearest dressing can be only a couple of metres out. Reserve room for
		// the whole overlay stack above the near clip, or the deepest layers would all clamp to the
		// same depth and z-fight.
		int32 DeepestOverlayOrder = 0;
		for (const USceneComponent* Child : CameraComponent->GetAttachChildren())
		{
			if (Child && Child->ComponentHasTag(DayArtForegroundTag))
			{
				DeepestOverlayOrder = FMath::Max(DeepestOverlayOrder,
					DayArtLayerOrder(Child->ComponentTags, DayArtForegroundOrderPrefix));
			}
		}
		NearDepth = FMath::Max(NearDepth - DayArtForegroundDepthMargin,
			DayArtForegroundMinDepth + DeepestOverlayOrder * DayArtBackdropLayerStep);

		// Camera-local: +X is the view axis, +Y screen right, +Z screen up. A mesh plane's normal
		// is its local +Z; a UMG quad's is its local +X, so each kind needs its own facing and
		// only the Widget's baked rotation is trusted here.
		const FRotator FaceCamera =
			FRotationMatrix::MakeFromZX(FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f)).Rotator();
		auto PlaceLayer = [&](USceneComponent& Layer, const float Depth)
		{
			const float FrameWidth = DayCameraFrameWidthAtDepth(*CameraComponent, Depth);
			if (FrameWidth <= 0.0f)
			{
				return;
			}
			// A UMG layer's quad is its draw size in local centimetres, and the setup script
			// already aimed its one visible face at the camera, so only depth and fill are left.
			const UWidgetComponent* Widget = Cast<UWidgetComponent>(&Layer);
			const FVector2D WidgetPage = Widget ? Widget->GetCurrentDrawSize() : FVector2D::ZeroVector;
			const float LayerWidth = WidgetPage.X > 0.0f ? WidgetPage.X : DayArtBackdropPlaneSize;
			const float LayerHeight = WidgetPage.Y > 0.0f ? WidgetPage.Y : DayArtBackdropPlaneSize;
			const FRotator LayerRotation = Widget ? Layer.GetRelativeRotation() : FaceCamera;
			Layer.SetRelativeTransform(FTransform(
				LayerRotation,
				FVector(Depth, 0.0f, 0.0f),
				FVector(FrameWidth / LayerWidth,
					FrameWidth / AspectRatio / LayerHeight,
					1.0f)));
		};

		for (USceneComponent* Child : CameraComponent->GetAttachChildren())
		{
			if (!Child)
			{
				continue;
			}
			if (Child->ComponentHasTag(DayArtBackdropTag))
			{
				// Order 0 (street) sits furthest; each later layer steps toward the camera so the
				// translucent stack sorts back-to-front without touching the stall or customers.
				PlaceLayer(*Child, FarDepth
					- DayArtLayerOrder(Child->ComponentTags, DayArtBackdropOrderPrefix) * DayArtBackdropLayerStep);
			}
			else if (Child->ComponentHasTag(DayArtForegroundTag))
			{
				// Order 0 (coins) sits just ahead of the nearest dressing; higher orders step
				// nearer still so the rope draws over the coins, all in front of the stall.
				PlaceLayer(*Child, NearDepth
					- DayArtLayerOrder(Child->ComponentTags, DayArtForegroundOrderPrefix) * DayArtBackdropLayerStep);
			}
		}
	}

	/**
	 * World height every portrait sprite gets. The PNGs differ in pixel size but share a
	 * baseline, so one height keeps the cast in proportion while leaving their own height
	 * differences intact.
	 */
	constexpr float DayArtPortraitSpriteHeight = 330.0f;

	/** Sprites have to resolve behind the stall front, not in front of it. */
	constexpr float DayArtPortraitDepthBias = 120.0f;

	/** Pointer proxy scale that keeps the whole visible portrait clickable for deliveries. */
	const FVector DayArtSeatProxyScale(1.8f, 1.8f, 2.6f);

	/**
	 * The plate row is authored with four slots while the stage decides how many seats are
	 * actually open, so smaller counts leave spare plates for the seats to be drawn from.
	 */
	constexpr int32 DayArtCustomerPlateCount = 4;

	/**
	 * FSpriteSceneProxy draws a quad of half-height 0.25 * ComponentScale * texture height, and
	 * the portrait PNGs differ in pixel size, so the scale is solved per texture.
	 */
	float DayPortraitSpriteScale(
		const UTexture2D* Texture,
		const float WorldHeight = DayArtPortraitSpriteHeight)
	{
		float TextureHeight = Texture ? static_cast<float>(Texture->GetSizeY()) : 0.0f;
#if WITH_EDITORONLY_DATA
		// In editor/PIE, async texture compilation can temporarily expose the engine's 32x32
		// default resource through GetSizeY(). The imported source stays authoritative and avoids
		// permanently baking that transient size into the portrait component scale.
		if (Texture && Texture->Source.IsValid() && Texture->Source.GetSizeY() > 0)
		{
			TextureHeight = static_cast<float>(Texture->Source.GetSizeY());
		}
#endif
		if (TextureHeight <= 0.0f)
		{
			return 1.0f;
		}
		return 2.0f * WorldHeight / TextureHeight;
	}

	/**
	 * Fraction of a portrait's height that is fully transparent along its bottom edge. The PNGs
	 * pad their canvas by different amounts - about 12% for the townsfolk, 4% for the special
	 * guests - so lining the image border up with the counter leaves part of the cast hovering.
	 * Scanned once per texture; a texture we cannot read reports no padding, which is the plain
	 * image-border alignment.
	 */
	float DayPortraitBottomPadding(UTexture2D* Texture)
	{
		if (!Texture)
		{
			return 0.0f;
		}

		static TMap<TWeakObjectPtr<UTexture2D>, float> PaddingCache;
		if (const float* Cached = PaddingCache.Find(Texture))
		{
			return *Cached;
		}

		// Alpha is the last byte of a BGRA8 pixel; a row is content as soon as one pixel shows.
		const auto ScanBottomPadding = [](const uint8* Pixels, const int32 Width, const int32 Height)
		{
			for (int32 Row = Height - 1; Row >= 0; --Row)
			{
				for (int32 Column = 0; Column < Width; ++Column)
				{
					if (Pixels[(static_cast<int64>(Row) * Width + Column) * 4 + 3] > 8)
					{
						return static_cast<float>(Height - 1 - Row) / static_cast<float>(Height);
					}
				}
			}
			return 0.0f;
		};

		float Padding = 0.0f;
#if WITH_EDITOR
		FTextureSource& Source = Texture->Source;
		TArray64<uint8> SourceMip;
		if (Source.IsValid() && Source.GetFormat() == TSF_BGRA8 && Source.GetMipData(SourceMip, 0))
		{
			Padding = ScanBottomPadding(SourceMip.GetData(), Source.GetSizeX(), Source.GetSizeY());
		}
#endif
		if (Padding <= 0.0f)
		{
			// Cooked builds have no source art, so read the mip the renderer uses instead.
			FTexturePlatformData* PlatformData = Texture->GetPlatformData();
			if (PlatformData && PlatformData->PixelFormat == PF_B8G8R8A8 && PlatformData->Mips.Num() > 0)
			{
				FTexture2DMipMap& Mip = PlatformData->Mips[0];
				const int64 ExpectedSize = static_cast<int64>(Mip.SizeX) * Mip.SizeY * 4;
				if (Mip.BulkData.GetBulkDataSize() >= ExpectedSize && ExpectedSize > 0)
				{
					if (const uint8* Pixels = static_cast<const uint8*>(Mip.BulkData.LockReadOnly()))
					{
						Padding = ScanBottomPadding(Pixels, Mip.SizeX, Mip.SizeY);
					}
					Mip.BulkData.Unlock();
				}
			}
		}

		PaddingCache.Add(Texture, Padding);
		return Padding;
	}

	/**
	 * Where a customer portrait can stand: one point per dish plate, screen-left first. The
	 * sprite is centred on the point, so the returned height puts the image's bottom border on
	 * the plate row's top edge, and the depth keeps it behind the stall.
	 * Returns empty when the plate row or the camera is missing, which keeps the whitebox layout.
	 */
	TArray<FVector> SolveDayArtPlateSlots(UWorld* World, const int32 SeatCount, FDayCameraFrame* OutFrame = nullptr)
	{
		TArray<FVector> Slots;
		const FDayArtPiece Plates = FindDayArtPiece(World, DayArtCustomerPlatesTag);
		FDayCameraFrame Frame;
		if (!Plates.IsValid() || SeatCount <= 0 || !TryGetDayCameraFrame(World, Frame))
		{
			return Slots;
		}
		if (OutFrame)
		{
			*OutFrame = Frame;
		}

		FVector PlatesOrigin = FVector::ZeroVector;
		FVector PlatesExtent = FVector::ZeroVector;
		Plates.GetBounds(PlatesOrigin, PlatesExtent);

		const int32 SlotCount = FMath::Max(SeatCount, DayArtCustomerPlateCount);
		const float SlotWidth = PlatesExtent.X * 2.0f / static_cast<float>(SlotCount);
		const float PlateBackY = PlatesOrigin.Y + PlatesExtent.Y;
		const float PlateTopZ = PlatesOrigin.Z + PlatesExtent.Z;

		Slots.Reserve(SlotCount);
		for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			// Slot 0 is screen left, which is the far +X end under the Yaw 90 camera.
			const float PlateX = PlatesOrigin.X + PlatesExtent.X
				- SlotWidth * (static_cast<float>(SlotIndex) + 0.5f);
			const FVector PlateAnchor = Frame.ToFrame(FVector(PlateX, PlateBackY, PlateTopZ));
			Slots.Add(Frame.ToWorld(FVector(
				PlateAnchor.X,
				PlateAnchor.Y + DayArtPortraitSpriteHeight * 0.5f,
				PlateAnchor.Z + DayArtPortraitDepthBias)));
		}
		return Slots;
	}

	int32 NearestPlateSlot(const TArray<FVector>& Slots, const FVector& Location)
	{
		int32 Nearest = INDEX_NONE;
		float NearestDistanceSq = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			const float DistanceSq = FVector::DistSquared(Slots[Index], Location);
			if (DistanceSq < NearestDistanceSq)
			{
				NearestDistanceSq = DistanceSq;
				Nearest = Index;
			}
		}
		return Nearest;
	}

	/** Random distinct plate per seat, so a fresh day does not always fill the same plates. */
	TArray<int32> ShuffledPlateSlotOrder(const int32 SlotCount)
	{
		TArray<int32> Order;
		Order.Reserve(SlotCount);
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			Order.Add(Index);
		}
		for (int32 Index = Order.Num() - 1; Index > 0; --Index)
		{
			Order.Swap(Index, FMath::RandHelper(Index + 1));
		}
		return Order;
	}

	/**
	 * Occupied portraits stay on the plate their customer arrived at; free seats are shuffled
	 * through the plates nobody is using, which is what makes the next arrival land on a random
	 * plate instead of a fixed one. Also re-snaps to the current slots so moving the camera or
	 * the art keeps the row aligned, and drops each sprite by its own transparent bottom padding
	 * so every customer stands on the counter edge rather than over it.
	 */
	void PlaceDayArtSeats(UWorld* World, const TArray<ASDayCharacterStandIn*>& Seats)
	{
		FDayCameraFrame Frame;
		const TArray<FVector> Slots = SolveDayArtPlateSlots(World, Seats.Num(), &Frame);
		if (Slots.IsEmpty())
		{
			return;
		}

		TArray<bool> Taken;
		Taken.Init(false, Slots.Num());
		TArray<ASDayCharacterStandIn*> FreeSeats;
		for (ASDayCharacterStandIn* Seat : Seats)
		{
			if (!Seat)
			{
				continue;
			}
			if (!Seat->bOccupied)
			{
				FreeSeats.Add(Seat);
				continue;
			}
			const int32 Slot = NearestPlateSlot(Slots, Seat->GetActorLocation());
			if (Slots.IsValidIndex(Slot) && !Taken[Slot])
			{
				Taken[Slot] = true;
				Seat->SetActorLocation(Slots[Slot]);
			}
			else
			{
				FreeSeats.Add(Seat);
			}
		}

		TArray<int32> Available;
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			if (!Taken[Index])
			{
				Available.Add(Index);
			}
		}
		for (ASDayCharacterStandIn* Seat : FreeSeats)
		{
			if (Available.IsEmpty())
			{
				break;
			}
			const int32 Pick = FMath::RandHelper(Available.Num());
			Seat->SetActorLocation(Slots[Available[Pick]]);
			Available.RemoveAtSwap(Pick);
		}

		// Seat actors carry no rotation here, so a relative offset is a straight world offset.
		for (ASDayCharacterStandIn* Seat : Seats)
		{
			if (Seat && Seat->Portrait)
			{
				const float Padding = DayPortraitBottomPadding(Seat->Portrait->Sprite);
				Seat->Portrait->SetRelativeLocation(
					Frame.Up * (-Padding * DayArtPortraitSpriteHeight));
			}
		}
	}
#pragma endregion K2 moonyfli

	FLinearColor IngredientColor(const FName IngredientId)
	{
		if (IngredientId == DayLingGuId) return FLinearColor(0.98f, 0.14f, 0.28f);
		if (IngredientId == DayYinShanJunId) return FLinearColor(0.10f, 0.78f, 0.64f);
		if (IngredientId == DayChiYanJiaoId) return FLinearColor(1.00f, 0.35f, 0.05f);
		if (IngredientId == DayYueLinYuId) return FLinearColor(0.10f, 0.42f, 0.95f);
		if (IngredientId == DayXuanYuQinId) return FLinearColor(0.72f, 0.18f, 0.90f);
		return FLinearColor::White;
	}

	FString IngredientDisplayName(const UObject* WorldContext, const FName IngredientId)
	{
		if (WorldContext)
		{
			if (const USChefGameInstance* GameInstance = WorldContext->GetWorld()
				? WorldContext->GetWorld()->GetGameInstance<USChefGameInstance>()
				: nullptr)
			{
				const FString Resolved = GameInstance->ResolveIngredientDisplayName(IngredientId);
				if (!Resolved.IsEmpty())
				{
					return Resolved;
				}
			}
		}
		if (IngredientId == DayLingGuId) return TEXT("煲仔饭");
		if (IngredientId == DayYinShanJunId) return TEXT("鸡蛋灌饼");
		if (IngredientId == DayChiYanJiaoId) return TEXT("九转脆肠");
		if (IngredientId == DayYueLinYuId) return TEXT("仰望星空派");
		if (IngredientId == DayXuanYuQinId) return TEXT("蔬菜汁鹅腿");
		return IngredientId.ToString();
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

#pragma region K2 moonyfli
	UDataTable* LoadSharedDayTable(const TCHAR* AssetName)
	{
		return LoadObject<UDataTable>(
			nullptr,
			*FString::Printf(
				TEXT("/Game/Shared/Data/%s.%s"),
				AssetName,
				AssetName));
	}

	UDataTable* LoadDayDataTable(const TCHAR* AssetName)
	{
		return LoadObject<UDataTable>(
			nullptr,
			*FString::Printf(
				TEXT("/Game/Day/Data/%s.%s"),
				AssetName,
				AssetName));
	}

	FSDayDishIconTuneRow ResolveDishIconTune(const USDayBoardVisualConfig* Config)
	{
		FSDayDishIconTuneRow BuiltIn;
		UDataTable* Table = Config ? Config->DishIconTune.LoadSynchronous() : nullptr;
		if (!Table)
		{
			Table = LoadDayDataTable(TEXT("DT_SDayDishIconTune"));
		}
		if (!Table)
		{
			return BuiltIn;
		}
		if (const FSDayDishIconTuneRow* Row = Table->FindRow<FSDayDishIconTuneRow>(
			TEXT("Default"),
			TEXT("ResolveDishIconTune"),
			false))
		{
			return *Row;
		}
		TArray<FSDayDishIconTuneRow*> Rows;
		Table->GetAllRows(TEXT("ResolveDishIconTune"), Rows);
		if (Rows.Num() > 0 && Rows[0])
		{
			return *Rows[0];
		}
		return BuiltIn;
	}

	UTexture2D* KeepDayFoodTextureReady(UTexture2D* Icon)
	{
#if PLATFORM_ANDROID
		if (Icon)
		{
			// Android can return a synchronously loaded texture before its first mobile mip is
			// ready for either Slate or a world-space material. Keep it resident through the
			// immediate presentation and wait so newly merged dish levels render on first use.
			Icon->SetForceMipLevelsToBeResident(2.0f);
			Icon->WaitForStreaming();
		}
#endif
		return Icon;
	}

	UTexture2D* ResolveIngredientIcon(const FName IngredientId)
	{
		if (UDataTable* Table = LoadSharedDayTable(TEXT("DT_Ingredients")))
		{
			if (const FSIngredientDefRow* Row = Table->FindRow<FSIngredientDefRow>(
				IngredientId,
				TEXT("ResolveIngredientIcon"),
				false))
			{
				if (UTexture2D* Icon = Row->Icon.LoadSynchronous())
				{
					return KeepDayFoodTextureReady(Icon);
				}
			}
		}

		const TCHAR* Fallback = nullptr;
		if (IngredientId == DayLingGuId) Fallback = TEXT("/Game/Day/Art/food/food_rice_V0.food_rice_V0");
		else if (IngredientId == DayYinShanJunId) Fallback = TEXT("/Game/Day/Art/food/food_egg_V0.food_egg_V0");
		else if (IngredientId == DayChiYanJiaoId) Fallback = TEXT("/Game/Day/Art/food/food_hand_V0.food_hand_V0");
		else if (IngredientId == DayYueLinYuId) Fallback = TEXT("/Game/Day/Art/food/food_fish_V0.food_fish_V0");
		else if (IngredientId == DayXuanYuQinId) Fallback = TEXT("/Game/Day/Art/food/food_leg_V0.food_leg_V0");
		return Fallback ? KeepDayFoodTextureReady(LoadObject<UTexture2D>(nullptr, Fallback)) : nullptr;
	}

	/** Artwork stem shipped for each chain: /Game/Day/Art/food/food_<stem>_V<level>. */
	FName DefaultDishArtStem(const FName IngredientId)
	{
		if (IngredientId == DayLingGuId) return TEXT("rice");
		if (IngredientId == DayYinShanJunId) return TEXT("egg");
		if (IngredientId == DayChiYanJiaoId) return TEXT("hand");
		if (IngredientId == DayYueLinYuId) return TEXT("fish");
		if (IngredientId == DayXuanYuQinId) return TEXT("leg");
		return NAME_None;
	}

	UTexture2D* LoadDishIconByName(const FName AssetName)
	{
		static TMap<FName, TWeakObjectPtr<UTexture2D>> IconCache;
		static TSet<FName> MissingIcons;
		if (MissingIcons.Contains(AssetName))
		{
			return nullptr;
		}
		if (const TWeakObjectPtr<UTexture2D>* Cached = IconCache.Find(AssetName))
		{
			if (Cached->IsValid())
			{
				return KeepDayFoodTextureReady(Cached->Get());
			}
		}

		const FString Name = AssetName.ToString();
		UTexture2D* Loaded = LoadObject<UTexture2D>(
			nullptr,
			*FString::Printf(TEXT("/Game/Day/Art/food/%s.%s"), *Name, *Name));
		Loaded = KeepDayFoodTextureReady(Loaded);
		if (Loaded)
		{
			IconCache.Add(AssetName, Loaded);
		}
		else
		{
			MissingIcons.Add(AssetName);
		}
		return Loaded;
	}

	UTexture2D* ResolveDishIcon(
		const USDayBoardVisualConfig* Config,
		const FName IngredientId,
		const int32 Level)
	{
		const int32 ClampedLevel = FMath::Clamp(Level, 0, 4);

		FName Stem = NAME_None;
		if (Config)
		{
			if (const FSDayDishIconSet* Set = Config->DishIconOverrides.Find(IngredientId))
			{
				if (Set->LevelIcons.IsValidIndex(ClampedLevel))
				{
					if (UTexture2D* Override = Set->LevelIcons[ClampedLevel].LoadSynchronous())
					{
						return Override;
					}
				}
			}
			if (const FName* Mapped = Config->DishArtStemByIngredient.Find(IngredientId))
			{
				Stem = *Mapped;
			}
		}
		if (Stem.IsNone())
		{
			Stem = DefaultDishArtStem(IngredientId);
		}
		if (Stem.IsNone())
		{
			return nullptr;
		}

		return LoadDishIconByName(
			FName(*FString::Printf(TEXT("food_%s_V%d"), *Stem.ToString(), ClampedLevel)));
	}

	bool GetViewPoint(const AActor* Context, FVector& OutLocation, FRotator& OutRotation)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
		if (!PlayerController || !PlayerController->GetViewTarget())
		{
			return false;
		}
		PlayerController->GetPlayerViewPoint(OutLocation, OutRotation);
		return true;
	}

	UTexture2D* ResolveCustomerPortrait(const FString& DisplayName)
	{
		if (UDataTable* Table = LoadSharedDayTable(TEXT("DT_CustomerNames")))
		{
			TArray<FSCustomerNameRow*> Rows;
			Table->GetAllRows(TEXT("ResolveCustomerPortrait"), Rows);
			for (const FSCustomerNameRow* Row : Rows)
			{
				if (Row && Row->DisplayName == DisplayName)
				{
					return Row->Portrait.LoadSynchronous();
				}
			}
		}
		return nullptr;
	}

	UTexture2D* ResolveSpecialNpcPortrait(const FName NpcId)
	{
		FName LookupId = NpcId;
		if (NpcId == DayNpcALingId)
		{
			LookupId = TEXT("NpcA");
		}
		else if (NpcId == DayNpcSangPoId)
		{
			LookupId = TEXT("NpcB");
		}

		if (UDataTable* Table = LoadSharedDayTable(TEXT("DT_SpecialNpcs")))
		{
			if (const FSSpecialNpcDefRow* Row = Table->FindRow<FSSpecialNpcDefRow>(
				LookupId,
				TEXT("ResolveSpecialNpcPortrait"),
				false))
			{
				return Row->Portrait.LoadSynchronous();
			}
		}
		return nullptr;
	}
#pragma endregion K2 moonyfli

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

TSharedRef<SWidget> USDayDragPreview::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		DragImage = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			TEXT("DayDragPreviewImage"));
		// Keep the drag preview in the texture's authored orientation. The world-space dish
		// plane applies its own horizontal correction when it is scaled below.
		DragImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		WidgetTree->RootWidget = DragImage;
	}

	return Super::RebuildWidget();
}

void USDayDragPreview::ShowPreview(
	UTexture2D* Texture,
	const FVector2D& ScreenPosition,
	const FVector2D& PreviewSize)
{
	if (!Texture)
	{
		HidePreview();
		return;
	}

	if (!DragImage)
	{
		TakeWidget();
	}
	if (!DragImage)
	{
		return;
	}

	DragImage->SetBrushFromTexture(Texture, true);
	DragImage->SetDesiredSizeOverride(PreviewSize);
	SetDesiredSizeInViewport(PreviewSize);
	// Centre horizontally and lift the preview above a finger so the touch point stays visible.
	SetAlignmentInViewport(FVector2D(0.5f, 1.15f));
	MovePreview(ScreenPosition);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USDayDragPreview::MovePreview(const FVector2D& ScreenPosition)
{
	// Pointer positions arrive in physical viewport pixels; let UMG remove the DPI scale.
	SetPositionInViewport(ScreenPosition, true);
}

void USDayDragPreview::HidePreview()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

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

	PieceIcon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PieceIcon"));
	PieceIcon->SetupAttachment(Root);
	PieceIcon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PieceIcon->SetCastShadow(false);
	PieceIcon->SetVisibility(false);

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

void ASDayCellVisual::SetInteractionRadius(const float InRadius, const bool bRound)
{
	const float CellScale = FMath::Max(1.0f, InRadius) / 50.0f;
	CellMesh->SetRelativeScale3D(FVector(CellScale, bRound ? CellScale : CellScale * 0.82f, 0.12f));
}

#pragma region K2 moonyfli
void ASDayCellVisual::SetSeatedInWell(const bool bInSeated)
{
	// Tracked as a tag, matching how the rest of the Day art binding marks intent on actors.
	if (bInSeated)
	{
		Tags.AddUnique(DayCellSeatedTag);
	}
	else
	{
		Tags.Remove(DayCellSeatedTag);
	}
	RefreshVisual();
}

void ASDayCellVisual::SetUseAuthoredVisuals(const bool bInUse)
{
	bUseAuthoredVisuals = bInUse;
	RefreshVisual();
}

void ASDayCellVisual::SetDishIconConfig(USDayBoardVisualConfig* InConfig)
{
	IconConfig = InConfig;

	UStaticMesh* Mesh = InConfig ? InConfig->DishIconMesh.LoadSynchronous() : nullptr;
	if (!Mesh)
	{
		Mesh = LoadBasicShape(TEXT("/Engine/BasicShapes/Plane.Plane"));
	}
	PieceIcon->SetStaticMesh(Mesh);

	UMaterialInterface* Parent = InConfig ? InConfig->DishIconMaterial.LoadSynchronous() : nullptr;
	if (!Parent)
	{
		Parent = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/Day/Materials/M_SDayDishIcon.M_SDayDishIcon"));
	}
	DishIconMaterial = Parent ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr;
	if (DishIconMaterial)
	{
		PieceIcon->SetMaterial(0, DishIconMaterial);
	}

	RefreshVisual();
}

void ASDayCellVisual::SetDragIconWorldLocation(const FVector& InWorldLocation)
{
	if (PieceIcon && PieceIcon->IsVisible())
	{
		PieceIcon->SetWorldLocation(InWorldLocation);
	}
}

#pragma endregion K2 moonyfli

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
		PieceIcon->SetVisibility(false);
		return;
	}

	FSDishPiece Piece;
	const bool bHasPiece = MergeBoard->TryGetPiece(CellIndex, Piece);
	const bool bSelected = MergeBoard->GetActiveDragCellIndex() == CellIndex;
	PieceMesh->SetVisibility(bHasPiece && !bUseAuthoredVisuals);
	PieceLabel->SetVisibility(bHasPiece && !bUseAuthoredVisuals);
	PieceIcon->SetVisibility(false);
	CellMesh->SetCustomDepthStencilValue(bSelected ? 2 : 1);
	CellMesh->SetRenderCustomDepth(bSelected);
	// The whitebox has no outline post-process, so the selection reads through tint and lift instead.
	ApplyTint(CellMesh, bSelected ? SelectedCellColor() : IdleCellColor());

	if (!bHasPiece)
	{
		return;
	}

	const float SelectionBoost = bSelected ? 1.25f : 1.0f;
#pragma region K2 moonyfli
	// Follow the cell radius the way CellMesh does, so a dish fills the pan's well instead of
	// reading as a dot in the middle of it. The 50 reference keeps the whitebox sizing intact.
	const float RadiusScale = VisualRadius / 50.0f;
	const float LevelScale = (0.52f + static_cast<float>(Piece.Level) * 0.10f) * SelectionBoost * RadiusScale;
	const float HeightScale = (0.25f + Piece.Level * 0.05f) * SelectionBoost * RadiusScale;
	PieceMesh->SetRelativeScale3D(FVector(LevelScale, LevelScale, HeightScale));
	// Seated in a well the dish only clears its own half-height; on a whitebox disc it also has
	// to clear the disc, and the selection lift doubles as the "picked up" cue.
	const float SeatedLift = HeightScale * 50.0f;
	const bool bSeatedInWell = ActorHasTag(DayCellSeatedTag);
	PieceMesh->SetRelativeLocation(FVector(
		0.0f,
		0.0f,
		bSeatedInWell
			? (bSelected ? SeatedLift * 2.0f : SeatedLift)
			: (bSelected ? 62.0f : 32.0f) * RadiusScale));
#pragma endregion K2 moonyfli

	const FLinearColor PieceColor = IngredientColor(Piece.IngredientId);
	ApplyTint(PieceMesh, bSelected ? PieceColor * 1.8f + FLinearColor(0.15f, 0.15f, 0.05f, 0.0f) : PieceColor);

	const USDayBoardVisualConfig* Config = IconConfig.Get();
	UTexture2D* DishIcon = (!Config || Config->bUseDishIcons)
		? ResolveDishIcon(Config, Piece.IngredientId, Piece.Level)
		: nullptr;
	if (DishIcon && DishIconMaterial)
	{
		const FSDayDishIconTuneRow Tune = ResolveDishIconTune(Config);
		const FName TextureParameter = Config ? Config->DishIconTextureParameter : FName(TEXT("Tex"));
		const float IconSize = Tune.WorldSize
			* (1.0f + Tune.ScalePerLevel * static_cast<float>(Piece.Level))
			* (bSelected ? Tune.SelectedScale : 1.0f);
		DishIconMaterial->SetTextureParameterValue(TextureParameter, DishIcon);
		// The camera sees the engine plane from its reverse UV side. Negating local X restores
		// the texture's authored left/right orientation without rotating it upside down.
		const float IconScale = IconSize / 100.0f;
		PieceIcon->SetRelativeScale3D(FVector(-IconScale, IconScale, IconScale));
		PieceIcon->SetRelativeRotation(FRotator(0.0f, Tune.Yaw, 0.0f));
		PieceIcon->SetRelativeLocation(Tune.LocalOffset);
		const float Push = Tune.CameraPush + (bSelected ? Tune.SelectedLift : 0.0f);
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		if (Push > KINDA_SMALL_NUMBER && GetViewPoint(this, ViewLocation, ViewRotation))
		{
			PieceIcon->AddWorldOffset(-ViewRotation.Vector() * Push);
		}
		PieceIcon->SetVisibility(true);
		PieceMesh->SetVisibility(false);
		PieceLabel->SetVisibility(Config && Config->bShowPieceLabelWithIcon);
	}

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

void ASDayIngredientBinVisual::Configure(
	const int32 InBinIndex,
	const FName InIngredientId,
	const FString& InDisplayName)
{
	BinIndex = InBinIndex;
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

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(Root);

	PortraitMotionRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PortraitMotionRoot"));
	PortraitMotionRoot->SetupAttachment(VisualRoot);

	CharacterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CharacterMesh"));
	CharacterMesh->SetupAttachment(VisualRoot);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CharacterMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CharacterMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

#pragma region K2 moonyfli
	Portrait = CreateDefaultSubobject<UBillboardComponent>(TEXT("Portrait"));
	Portrait->SetupAttachment(PortraitMotionRoot);
	Portrait->SetRelativeLocation(FVector(0.0f, 0.0f, 92.0f));
	Portrait->bIsScreenSizeScaled = false;
	Portrait->SetHiddenInGame(false);
	Portrait->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Portrait->SetVisibility(false);

	// Keep this independent of Portrait's per-texture scale and transparent-canvas offset. It still
	// follows authored entry/wobble motion, and delivery captures it before the eat timeline starts.
	RevenueFlyAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("RevenueFlyAnchor"));
	RevenueFlyAnchor->SetupAttachment(PortraitMotionRoot);
	RevenueFlyAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));

	EatEffectAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("EatEffectAnchor"));
	EatEffectAnchor->SetupAttachment(VisualRoot);
	EatEffectAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));

	GiftEffectAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("GiftEffectAnchor"));
	GiftEffectAnchor->SetupAttachment(VisualRoot);
	GiftEffectAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));
#pragma endregion K2 moonyfli

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(VisualRoot);
	// -Y is screen-down under the portrait camera; the seats hug the top edge, so the
	// name plate goes underneath them where there is empty space.
	Label->SetRelativeLocation(FVector(0.0f, -120.0f, 150.0f));
	Label->SetRelativeRotation(LabelFacingRotation());
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextCenter);
	Label->SetWorldSize(24.0f);
}

#pragma region K2 moonyfli
void ASDayCharacterStandIn::EnsurePortraitMotionAttachment()
{
	if (Portrait
		&& PortraitMotionRoot
		&& Portrait->GetAttachParent() != PortraitMotionRoot)
	{
		// Derived seat Blueprints created before PortraitMotionRoot existed can retain VisualRoot as
		// the serialized parent. Keep the authored portrait offset while repairing that stale link.
		Portrait->AttachToComponent(
			PortraitMotionRoot,
			FAttachmentTransformRules::KeepRelativeTransform);
	}
}

void ASDayCharacterStandIn::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsurePortraitMotionAttachment();

	if (!bSceneAuthoredSeat)
	{
		return;
	}

	Tags.AddUnique(TEXT("SDay.Seat"));
	const UWorld* World = GetWorld();
	if (bShowEditorPreview && (!World || !World->IsGameWorld()))
	{
		SetPortrait(EditorPreviewPortrait.LoadSynchronous());
		CharacterMesh->SetVisibility(false);
		Label->SetVisibility(false);
	}
}

void ASDayCharacterStandIn::BeginPlay()
{
	Super::BeginPlay();
	// Construction normally fixes the hierarchy, but enforce it once more after Blueprint
	// component instancing so runtime re-instancing cannot bypass the motion root.
	EnsurePortraitMotionAttachment();
	if (bSceneAuthoredSeat)
	{
		SetPortrait(nullptr);
		PresentedOccupantKey.Reset();
	}
}
#pragma endregion K2 moonyfli

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

#pragma region K2 moonyfli
void ASDayCharacterStandIn::SetPortrait(UTexture2D* InTexture)
{
	Portrait->SetSprite(InTexture);
	Portrait->SetRelativeScale3D(FVector(DayPortraitSpriteScale(InTexture, PortraitWorldHeight)));

	// Each portrait pads its canvas differently, so the seat lands the painted feet, not the
	// image border. Seats are authored upright, so their local -Z is the counter direction.
	const float BottomPadding = DayPortraitBottomPadding(InTexture);
	Portrait->SetRelativeLocation(
		PortraitLocalOffset - FVector(0.0f, 0.0f, BottomPadding * PortraitWorldHeight));
	Portrait->SetHiddenInGame(false);
	Portrait->SetVisibility(InTexture != nullptr);
}

void ASDayCharacterStandIn::SetSceneSeatEnabled(const bool bEnabled)
{
	bDeliveryTarget = bEnabled;
	SetActorHiddenInGame(!bEnabled);
	SetActorEnableCollision(bEnabled);
	if (!bEnabled)
	{
		bOccupied = false;
		NpcId = NAME_None;
		CustomerId.Reset();
		SetPortrait(nullptr);
		NotifySeatVacated();
	}
}

void ASDayCharacterStandIn::NotifySeatOccupied(
	const FString& OccupantKey,
	const bool bSpecialNpc,
	const FName IngredientId,
	const int32 Level,
	const FName GiftId)
{
	if (PresentedOccupantKey == OccupantKey && !bPresentationDepartureInProgress)
	{
		return;
	}

	if (!PresentedOccupantKey.IsEmpty())
	{
		CompletePresentationDeparture();
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PresentationDepartureFallbackHandle);
	}
	bHoldPresentationForServeAttempt = false;
	bPresentationDepartureInProgress = false;
	bPresentedSpecialNpc = bSpecialNpc;
	PresentedOccupantKey = OccupantKey;
	OnSeatOccupied(OccupantKey, bSpecialNpc, IngredientId, Level, GiftId);
}

void ASDayCharacterStandIn::NotifySeatVacated()
{
	if (PresentedOccupantKey.IsEmpty()
		|| bHoldPresentationForServeAttempt
		|| bPresentationDepartureInProgress)
	{
		return;
	}
	BeginPresentationDeparture(false, bPresentedSpecialNpc);
}

void ASDayCharacterStandIn::NotifyServeSucceeded(const bool bSpecialNpc)
{
	bHoldPresentationForServeAttempt = false;
	if (PresentedOccupantKey.IsEmpty())
	{
		return;
	}
	OnServeSucceeded(bSpecialNpc);
	BeginPresentationDeparture(true, bSpecialNpc);
}

void ASDayCharacterStandIn::BeginServeAttempt()
{
	if (!PresentedOccupantKey.IsEmpty() && !bPresentationDepartureInProgress)
	{
		bHoldPresentationForServeAttempt = true;
	}
}

void ASDayCharacterStandIn::CancelServeAttempt()
{
	bHoldPresentationForServeAttempt = false;
}

void ASDayCharacterStandIn::BeginPresentationDeparture(
	const bool bServed,
	const bool bSpecialNpc)
{
	if (PresentedOccupantKey.IsEmpty() || bPresentationDepartureInProgress)
	{
		return;
	}

	bHoldPresentationForServeAttempt = false;
	bPresentationDepartureInProgress = true;

	// Keep legacy cleanup behavior (dish/gift icons) separate from the new authored motion event.
	OnSeatVacated();
	OnDepartureRequested(bServed, bSpecialNpc);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PresentationDepartureFallbackHandle,
			this,
			&ASDayCharacterStandIn::CompletePresentationDeparture,
			FMath::Max(0.1f, DepartureFallbackSeconds),
			false);
	}
}

void ASDayCharacterStandIn::CompletePresentationDeparture()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PresentationDepartureFallbackHandle);
	}
	bHoldPresentationForServeAttempt = false;
	bPresentationDepartureInProgress = false;
	bPresentedSpecialNpc = false;
	PresentedOccupantKey.Reset();
	SetPortrait(nullptr);
}
#pragma endregion K2 moonyfli

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

#pragma region K2 moonyfli
void ASDayCameraRig::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

void ASDayCameraRig::RefitDayArt()
{
	// Intentionally empty. The cookingUI layers stay at their authored transforms so the
	// editor camera preview and PIE show the same picture.
}
#pragma endregion K2 moonyfli

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
	Camera->AspectRatio = DayBoardPresentationPrivate::DayPortraitAspectRatio; //add by K2

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
	IngredientBinFogSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/VFX_Merge/Niagara/BOX/Min_BoxOpen_Fog.Min_BoxOpen_Fog")));
	IngredientBinFoodBurstSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/VFX_Merge/Niagara/BOX/Min_BoxOpen_FoodBurst.Min_BoxOpen_FoodBurst")));
	IngredientBinTrailSystem = TSoftObjectPtr<UNiagaraSystem>(
		FSoftObjectPath(TEXT("/Game/VFX_Merge/Niagara/BOX/Min_BoxOpen_Trail.Min_BoxOpen_Trail")));

#pragma region K2 moonyfli
	auto MakeGhostFire = [](const TCHAR* ComponentName, const TCHAR* AssetPath, const float BiasX, const float BiasY)
	{
		FSDayGhostFireAnchor Anchor;
		Anchor.ComponentName = FName(ComponentName);
		Anchor.System = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(AssetPath));
		Anchor.FrameBias = FVector2D(BiasX, BiasY);
		return Anchor;
	};
	GhostFireAnchors.Reset();
	GhostFireAnchors.Add(MakeGhostFire(
		TEXT("NC_Ghost_TopLeft"),
		TEXT("/Game/VFX_Fire/Niagara/Min_NormalFire_LeftUp.Min_NormalFire_LeftUp"),
		-0.74f,
		0.72f));
	GhostFireAnchors.Add(MakeGhostFire(
		TEXT("NC_Ghost_TopRight"),
		TEXT("/Game/VFX_Fire/Niagara/Min_NormalFire_RightUp.Min_NormalFire_RightUp"),
		0.74f,
		0.70f));
	GhostFireAnchors.Add(MakeGhostFire(
		TEXT("NC_Ghost_Right"),
		TEXT("/Game/VFX_Fire/Niagara/Min_NormalFire_RightBottom.Min_NormalFire_RightBottom"),
		0.88f,
		0.08f));
	GhostFireAnchors.Add(MakeGhostFire(
		TEXT("NC_Ghost_BottomLeft"),
		TEXT("/Game/VFX_Fire/Niagara/Min_NormalFire_LeftBottom.Min_NormalFire_LeftBottom"),
		-0.72f,
		-0.62f));
#pragma endregion K2 moonyfli
}

void ASDayBoardPresenter::BeginPlay()
{
	Super::BeginPlay();
	LogicBoard = ASMergeBoard::FindBoard(this);
#pragma region K2 moonyfli
	DisableDayArtCollision(GetWorld());
#pragma endregion K2 moonyfli
	BuildWhitebox();
	EnsureDragPreview();

	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->OnSandboxStateChanged.AddUniqueDynamic(this, &ASDayBoardPresenter::RefreshFromLogic);
	}

	if (ASDayPlayerController* DayPlayerController =
		Cast<ASDayPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		DayPlayerController->RegisterBoardPresenter(this);
	}

	if (bTakeCameraOnBeginPlay)
	{
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
		{
#pragma region K2 moonyfli
			// The camera is a component now, so the view target is its owner: AActor::CalcCamera
			// adopts the first active camera component, and the key light has to follow the
			// component's rotation rather than the restaurant actor's.
			if (UCameraComponent* LevelCamera = DayBoardPresentationPrivate::FindDayCameraComponent(GetWorld()))
			{
				PlayerController->SetViewTarget(LevelCamera->GetOwner());
				DayBoardPresentationPrivate::AimKeyLightAlongCamera(
					WhiteboxKeyLight, LevelCamera->GetComponentRotation());
			}
			else
			{
				// Re-apply the portrait framing at runtime so Live Coding picks it up without an editor restart.
				Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
				Camera->SetRelativeLocation(FVector(0.0f, 60.0f, 2200.0f));
				Camera->SetRelativeRotation(FRotator(-90.0f, 90.0f, 0.0f));
				Camera->OrthoWidth = PortraitOrthoWidth;
				Camera->bConstrainAspectRatio = true;
				Camera->AspectRatio = DayBoardPresentationPrivate::DayPortraitAspectRatio;
				PlayerController->SetViewTarget(this);
			}
#pragma endregion K2 moonyfli
		}
	}
	RefreshFromLogic();
#pragma region K2 moonyfli
	EnsureGhostFireVfx();
#pragma endregion K2 moonyfli
}

void ASDayBoardPresenter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>())
	{
		GameInstance->OnSandboxStateChanged.RemoveDynamic(this, &ASDayBoardPresenter::RefreshFromLogic);
	}
	if (UWorld* World = GetWorld())
	{
		for (TPair<uint64, FTimerHandle>& Flight : IngredientFlightTimers)
		{
			World->GetTimerManager().ClearTimer(Flight.Value);
		}
	}
	IngredientFlightTimers.Reset();
	PendingIngredientArrivalCounts.Reset();
	if (DragPreview)
	{
		DragPreview->RemoveFromParent();
		DragPreview = nullptr;
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

	if (bUseExternalPointerDriver)
	{
		UpdateDraggedIcon(LastPointerPosition);
		return;
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
	if (bPointerDown)
	{
		UpdateDraggedIcon(ScreenPosition);
	}
	bPointerWasDown = bPointerDown;
}

void ASDayBoardPresenter::BuildWhitebox()
{
	USDayBoardVisualConfig* Config = VisualConfig.LoadSynchronous();
	UStaticMesh* Cube = LoadBasicShape(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = LoadBasicShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	const bool bUseDayArt = FindDayArtPiece(GetWorld(), DayArtBoardTag).IsValid(); //add by K2

	UMaterialInterface* BoardMaterial = Config ? Config->BoardMaterial.LoadSynchronous() : nullptr;
	UMaterialInterface* CellMaterial = Config ? Config->CellMaterial.LoadSynchronous() : nullptr;

	BoardFrame->SetVisibility(!bUseDayArt);
	Counter->SetVisibility(!bUseDayArt);
	if (!bUseDayArt)
	{
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

#pragma region K2 moonyfli
	// Whitebox fallback only. The shipping layout is DT_SDayBoardLayout, generated from the
	// wells modelled into the pan by Tools/GeneratePanCellLayout.py, so this just needs one
	// sane cell per logical index laid out flat across the whitebox board area.
	constexpr int32 FallbackColumns = 5;
	constexpr int32 FallbackRows = 5;
	for (int32 Y = 0; Y < FallbackRows; ++Y)
	{
		for (int32 X = 0; X < FallbackColumns; ++X)
		{
			FSDayBoardLayoutRow Row;
			Row.CellIndex = Y * FallbackColumns + X;
			Row.Transform = FTransform(
				FRotator::ZeroRotator,
				FVector(
					FMath::Lerp(-330.0f, 330.0f, X / float(FallbackColumns - 1)),
					FMath::Lerp(450.0f, -150.0f, Y / float(FallbackRows - 1)),
					35.0f),
				FVector::OneVector);
			Row.VisualRadius = 65.0f;
			Rows.Add(Row);
		}
	}
	return Rows;
#pragma endregion K2 moonyfli
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

#pragma region K2 moonyfli
	const FDayArtPiece ArtBoard = FindDayArtPiece(World, DayArtBoardTag);
	const bool bUseDayArt = ArtBoard.IsValid();
#pragma endregion K2 moonyfli

	const TArray<FSDayBoardLayoutRow> LayoutRows = GetLayoutRows();
	TArray<FTransform> SpawnTransforms;
	SpawnTransforms.Reserve(LayoutRows.Num());
	for (const FSDayBoardLayoutRow& Row : LayoutRows)
	{
		FTransform LocalTransform = Row.Transform;
		LocalTransform.SetLocation(MirrorX(Row.Transform.GetLocation()));
#pragma region K2 moonyfli
		FTransform SpawnTransform = LocalTransform * GetActorTransform();
		if (bUseDayArt)
		{
			// Art rows are pan-local and already oriented, so the mirror the whitebox needs
			// would move them off their wells.
			SpawnTransform = DayArtCellToWorld(ArtBoard.GetTransform(), Row.Transform);
		}
#pragma endregion K2 moonyfli
		SpawnTransforms.Add(SpawnTransform);
	}

	for (int32 RowIndex = 0; RowIndex < LayoutRows.Num(); ++RowIndex)
	{
		const FSDayBoardLayoutRow& Row = LayoutRows[RowIndex];
		const FTransform& SpawnTransform = SpawnTransforms[RowIndex];
		float InteractionRadius = Row.VisualRadius;
		if (bUseDayArt && LayoutRows.Num() > 1)
		{
			float NearestCellDistance = TNumericLimits<float>::Max();
			for (int32 OtherIndex = 0; OtherIndex < SpawnTransforms.Num(); ++OtherIndex)
			{
				if (OtherIndex != RowIndex)
				{
					NearestCellDistance = FMath::Min(
						NearestCellDistance,
						FVector::Distance(SpawnTransform.GetLocation(), SpawnTransforms[OtherIndex].GetLocation()));
				}
			}
			// Each cell takes half of its nearest-neighbour clearance. This also guarantees
			// every non-neighbour pair remains separated by at least CellHitZoneGap.
			InteractionRadius = FMath::Max(1.0f, (NearestCellDistance - CellHitZoneGap) * 0.5f);
		}

		FActorSpawnParameters Params;
		Params.Owner = this;
		ASDayCellVisual* Visual = World->SpawnActor<ASDayCellVisual>(
			VisualClass,
			SpawnTransform,
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
		Visual->SetDishIconConfig(Config);
		Visual->Configure(Row.CellIndex, Row.VisualRadius, LogicBoard.Get());
#pragma region K2 moonyfli
		// The art mesh owns the visible holes; keep the logical trace surface but remove
		// the old whitebox cylinder. Plated food remains represented by PieceIcon.
		Visual->CellMesh->SetVisibility(!bUseDayArt);
		if (bUseDayArt)
		{
			// The authored wells are round. Enlarge the invisible pointer surface up to the
			// nearest neighbouring cell while retaining a small, configurable safety gap.
			Visual->SetInteractionRadius(InteractionRadius, true);
			Visual->SetSeatedInWell(true);
		}
		Visual->SetUseAuthoredVisuals(bUseDayArt);
#pragma endregion K2 moonyfli
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
	for (int32 Index = 0; Index < DayIngredientBinCount; ++Index)
	{
		FName IngredientId = NAME_None;
		const FSDayIngredientBinOutput* BinOutput = nullptr;
		if (Config)
		{
			for (const FSDayIngredientBinOutput& Output : Config->IngredientBinOutputs)
			{
				if (Output.BinIndex == Index)
				{
					BinOutput = &Output;
					IngredientId = Output.IngredientId;
					break;
				}
			}
		}
		if (IngredientId.IsNone())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Day ingredient bin %d has no configured IngredientId; its hit zone remains but it will not spawn a piece."),
				Index);
		}

		const FVector Location(-380.0f + Index * 190.0f, -690.0f + FMath::Abs(2 - Index) * 18.0f, 45.0f);
#pragma region K2 moonyfli
		const FDayArtPiece ArtBin = FindDayArtPiece(World, DayArtBinTag(Index));
		FVector ArtBinCenter = FVector::ZeroVector;
		FVector ArtBinExtent = FVector::ZeroVector;
		FTransform SpawnTransform = FTransform(MirrorX(Location)) * GetActorTransform();
		if (ArtBin.IsValid())
		{
			ArtBin.GetBounds(ArtBinCenter, ArtBinExtent);
			SpawnTransform.SetLocation(ArtBinCenter);
		}
#pragma endregion K2 moonyfli
		FActorSpawnParameters Params;
		Params.Owner = this;
		ASDayIngredientBinVisual* Bin = World->SpawnActor<ASDayIngredientBinVisual>(
			BinClass,
			SpawnTransform,
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
		Bin->Configure(
			Index,
			IngredientId,
			IngredientId.IsNone() ? TEXT("未配置") : IngredientDisplayName(this, IngredientId));
#pragma region K2 moonyfli
		if (ArtBin.IsValid())
		{
			// The imported box supplies the visible geometry. This hidden cube is sized to
			// the art bounds and remains the stable Visibility trace target. Designers can
			// expand that target without changing the authored art or level placement.
			const float GlobalHitScale = Config ? Config->IngredientBinHitScale : 1.0f;
			const FVector PerBinHitScale = BinOutput ? BinOutput->HitScale : FVector::OneVector;
			const FVector HitOffset = BinOutput ? BinOutput->HitOffset : FVector::ZeroVector;
			Bin->BinMesh->SetRelativeScale3D((ArtBinExtent / 50.0f) * GlobalHitScale * PerBinHitScale);
			Bin->BinMesh->SetRelativeLocation(HitOffset);
			Bin->BinMesh->SetVisibility(false);
			Bin->Label->SetRelativeLocation(FVector(0.0f, 0.0f, ArtBinExtent.Z + 30.0f));
		}
#pragma endregion K2 moonyfli
		IngredientBins.Add(Bin);
	}
}

bool ASDayBoardPresenter::TryBindSceneAuthoredSeats(const int32 DesiredSeatCount)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	TArray<ASDayCharacterStandIn*> AuthoredSeats;
	for (TActorIterator<ASDayCharacterStandIn> It(World); It; ++It)
	{
		ASDayCharacterStandIn* Seat = *It;
		if (Seat && Seat->bSceneAuthoredSeat)
		{
			AuthoredSeats.Add(Seat);
		}
	}

	AuthoredSeats.Sort([](const ASDayCharacterStandIn& A, const ASDayCharacterStandIn& B)
	{
		return A.AuthoredSeatSlot < B.AuthoredSeatSlot;
	});

	if (AuthoredSeats.Num() < DesiredSeatCount)
	{
		for (ASDayCharacterStandIn* Seat : AuthoredSeats)
		{
			Seat->SetSceneSeatEnabled(false);
		}
		if (!AuthoredSeats.IsEmpty())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Day scene has %d authored seats but gameplay needs %d; using runtime fallback."),
				AuthoredSeats.Num(),
				DesiredSeatCount);
		}
		return false;
	}

	bUsingSceneAuthoredSeats = true;
	CharacterStandIns = AuthoredSeats;
	const bool bUseDayArt = FindDayArtPiece(World, DayArtBoardTag).IsValid();
	for (int32 Index = 0; Index < CharacterStandIns.Num(); ++Index)
	{
		ASDayCharacterStandIn* Seat = CharacterStandIns[Index];
		if (!Seat)
		{
			continue;
		}

		const bool bEnabled = Index < DesiredSeatCount;
		Seat->SeatIndex = bEnabled ? Index : INDEX_NONE;
		Seat->SetSceneSeatEnabled(bEnabled);
		if (!bEnabled)
		{
			continue;
		}

		Seat->SetLabelFont(ResolveLabelFont());
		Seat->Configure(TEXT("空座"), FLinearColor(0.95f, 0.75f, 0.65f));
		Seat->CharacterMesh->SetVisibility(!bUseDayArt);
		Seat->Label->SetVisibility(!bUseDayArt);
		if (bUseDayArt)
		{
			Seat->CharacterMesh->SetRelativeScale3D(DayArtSeatProxyScale);
		}
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Bound %d/%d scene-authored Day customer seats."),
		DesiredSeatCount,
		AuthoredSeats.Num());
	return true;
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
	bUsingSceneAuthoredSeats = false;
	if (TryBindSceneAuthoredSeats(DeliverySeatCount))
	{
		RefreshCharacters();
		return;
	}
	const bool bUseDayArt = FindDayArtPiece(World, DayArtBoardTag).IsValid(); //add by K2

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
	int32 ChefIndex = INDEX_NONE;
	if (!bUseDayArt)
	{
		ChefIndex = DeliverySeatCount;
		Labels.Add(TEXT("厨师"));
		Locations.Add(FVector(405.0f, -365.0f, 105.0f));
		Colors.Add(FLinearColor(0.92f, 0.92f, 0.88f));
	}
	// Art mode overrides the whitebox seat row with a random dish plate per seat.
	const TArray<FVector> PlateSlots = bUseDayArt
		? SolveDayArtPlateSlots(World, DeliverySeatCount)
		: TArray<FVector>();
	const TArray<int32> PlateSlotOrder = ShuffledPlateSlotOrder(PlateSlots.Num());
#pragma endregion K2 moonyfli

	for (int32 Index = 0; Index < Labels.Num(); ++Index)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
#pragma region K2 moonyfli
		const bool bUsePlateSlot = PlateSlotOrder.IsValidIndex(Index)
			&& PlateSlots.IsValidIndex(PlateSlotOrder[Index]);
		const FTransform SpawnTransform = bUsePlateSlot
			? FTransform(PlateSlots[PlateSlotOrder[Index]])
			: FTransform(MirrorX(Locations[Index])) * GetActorTransform();
#pragma endregion K2 moonyfli
		ASDayCharacterStandIn* Character = World->SpawnActor<ASDayCharacterStandIn>(
			CharacterClass,
			SpawnTransform,
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
		const bool bIsChef = Index == ChefIndex;
		Character->SeatIndex = bIsChef ? INDEX_NONE : Index;
		Character->bOccupied = false;
		Character->bDeliveryTarget = !bIsChef;
		Character->SetLabelFont(ResolveLabelFont());
		Character->Configure(Labels[Index], Colors[Index]);
#pragma region K2 moonyfli
		// PNG portraits replace the old whitebox cylinder. Keep its invisible trace proxy and
		// status label; the chef is still not spawned in art mode.
		Character->CharacterMesh->SetVisibility(!bUseDayArt);
		if (bUsePlateSlot)
		{
			// The actor already stands where the portrait belongs, so the sprite sits on the
			// origin and the pointer proxy grows to cover what the player actually sees.
			Character->Portrait->SetRelativeLocation(FVector::ZeroVector);
			Character->CharacterMesh->SetRelativeScale3D(DayArtSeatProxyScale);
			// The world label lies flat for the old top-down framing, so it degenerates into
			// noise over the customer's head here; the HUD carries the same order text.
			Character->Label->SetVisibility(false);
		}
#pragma endregion K2 moonyfli
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
			if (Seat->PresentedOccupantKey != Customer.CustomerId)
			{
				if (!Seat->PresentedOccupantKey.IsEmpty())
				{
					Seat->CompletePresentationDeparture();
				}
				Seat->SetPortrait(ResolveCustomerPortrait(Customer.DisplayName));
			}
			Seat->NotifySeatOccupied(
				Customer.CustomerId,
				false,
				Customer.Order.IngredientId,
				Customer.Order.Level,
				NAME_None);
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
			const FString NpcOccupantKey = SeatedNpc.NpcId.ToString();
			if (Seat->PresentedOccupantKey != NpcOccupantKey)
			{
				if (!Seat->PresentedOccupantKey.IsEmpty())
				{
					Seat->CompletePresentationDeparture();
				}
				Seat->SetPortrait(ResolveSpecialNpcPortrait(SeatedNpc.NpcId));
			}
			Seat->NotifySeatOccupied(
				NpcOccupantKey,
				true,
				SeatedNpc.Order.IngredientId,
				SeatedNpc.Order.Level,
				SeatedNpc.GiftId);
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
		Seat->NotifySeatVacated();
		const float Cooldown = CustomerDirector
			? CustomerDirector->GetSeatCooldownRemaining(SeatIndex)
			: 0.0f;
		Seat->SetHeadline(
			FString::Printf(TEXT("空座%d\n补客 %.0fs"), SeatIndex + 1, Cooldown),
			FLinearColor(0.62f, 0.58f, 0.54f));
#pragma endregion K2 moonyfli
	}

#pragma region K2 moonyfli
	// Occupancy and portraits are settled now, so the free (invisible) seats can move onto the
	// plates nobody uses; whichever plate a seat holds is where its next customer shows up.
	if (!bUsingSceneAuthoredSeats)
	{
		PlaceDayArtSeats(GetWorld(), Seats);
	}
#pragma endregion K2 moonyfli
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
			const int32 RevenueBefore = GetGameInstance<USChefGameInstance>()
				? GetGameInstance<USChefGameInstance>()->Revenue
				: 0;
			// The seat actor sits on the plate/collision proxy. Revenue feedback should visibly
			// originate from the customer artwork instead of that gameplay origin.
			const FVector RevenueSource = Character->RevenueFlyAnchor
				? Character->RevenueFlyAnchor->GetComponentLocation()
				: Character->GetActorLocation();
			Character->BeginServeAttempt();
			const bool bDelivered = Director->TryDeliverFromCellToCustomer(
				Board->GetActiveDragCellIndex(),
				Character->CustomerId);
			if (bDelivered)
			{
				const USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>();
				TArray<UUserWidget*> DayHudWidgets;
				UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, DayHudWidgets, USDayHUD::StaticClass(), true);
				if (DayHudWidgets.Num() > 0)
				{
					CastChecked<USDayHUD>(DayHudWidgets[0])->PlayRevenueFlyFromWorld(
						RevenueSource,
						GameInstance ? GameInstance->Revenue - RevenueBefore : 0);
				}
				Character->NotifyServeSucceeded(false);
			}
			else
			{
				Character->CancelServeAttempt();
			}
			return bDelivered;
		}
		return false;
	}

	if (ASSpecialNpcDirector* Director = ASSpecialNpcDirector::FindDirector(this))
	{
		const int32 RevenueBefore = GetGameInstance<USChefGameInstance>()
			? GetGameInstance<USChefGameInstance>()->Revenue
			: 0;
		const FVector RevenueSource = Character->RevenueFlyAnchor
			? Character->RevenueFlyAnchor->GetComponentLocation()
			: Character->GetActorLocation();
		Character->BeginServeAttempt();
		const bool bDelivered = Director->TryDeliverToNpc(Character->NpcId);
		if (bDelivered)
		{
			const USChefGameInstance* GameInstance = GetGameInstance<USChefGameInstance>();
			TArray<UUserWidget*> DayHudWidgets;
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, DayHudWidgets, USDayHUD::StaticClass(), true);
			if (DayHudWidgets.Num() > 0)
			{
				CastChecked<USDayHUD>(DayHudWidgets[0])->PlayRevenueFlyFromWorld(
					RevenueSource,
					GameInstance ? GameInstance->Revenue - RevenueBefore : 0);
			}
			Character->NotifyServeSucceeded(true);
		}
		else
		{
			Character->CancelServeAttempt();
		}
		return bDelivered;
	}
	return false;
}

ASDayCharacterStandIn* ASDayBoardPresenter::FindDeliveryPlateTarget(const FVector2D& ScreenPosition) const
{
	const FDayArtPiece Plates = FindDayArtPiece(GetWorld(), DayArtCustomerPlatesTag);
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!Plates.IsValid() || !PlayerController)
	{
		return nullptr;
	}

	FVector BoundsOrigin = FVector::ZeroVector;
	FVector BoundsExtent = FVector::ZeroVector;
	Plates.GetBounds(BoundsOrigin, BoundsExtent);
	FVector2D ScreenMin(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
	FVector2D ScreenMax(TNumericLimits<double>::Lowest(), TNumericLimits<double>::Lowest());
	bool bProjectedAnyCorner = false;
	for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
	{
		const FVector Corner(
			BoundsOrigin.X + ((CornerIndex & 1) ? BoundsExtent.X : -BoundsExtent.X),
			BoundsOrigin.Y + ((CornerIndex & 2) ? BoundsExtent.Y : -BoundsExtent.Y),
			BoundsOrigin.Z + ((CornerIndex & 4) ? BoundsExtent.Z : -BoundsExtent.Z));
		FVector2D ProjectedCorner = FVector2D::ZeroVector;
		if (UGameplayStatics::ProjectWorldToScreen(PlayerController, Corner, ProjectedCorner))
		{
			ScreenMin.X = FMath::Min(ScreenMin.X, ProjectedCorner.X);
			ScreenMin.Y = FMath::Min(ScreenMin.Y, ProjectedCorner.Y);
			ScreenMax.X = FMath::Max(ScreenMax.X, ProjectedCorner.X);
			ScreenMax.Y = FMath::Max(ScreenMax.Y, ProjectedCorner.Y);
			bProjectedAnyCorner = true;
		}
	}

	if (!bProjectedAnyCorner
		|| ScreenPosition.X < ScreenMin.X || ScreenPosition.X > ScreenMax.X
		|| ScreenPosition.Y < ScreenMin.Y || ScreenPosition.Y > ScreenMax.Y)
	{
		return nullptr;
	}

	const double ScreenWidth = ScreenMax.X - ScreenMin.X;
	if (ScreenWidth <= UE_SMALL_NUMBER)
	{
		return nullptr;
	}
	const int32 PlateSlotCount = FMath::Max(DayArtCustomerPlateCount, GetDeliverySeatCount());
	const int32 PlateSlot = FMath::Clamp(
		FMath::FloorToInt((ScreenPosition.X - ScreenMin.X) / ScreenWidth * PlateSlotCount),
		0,
		PlateSlotCount - 1);
	const TArray<FVector> PlateSlots = SolveDayArtPlateSlots(GetWorld(), PlateSlotCount);
	if (!PlateSlots.IsValidIndex(PlateSlot))
	{
		return nullptr;
	}

	for (ASDayCharacterStandIn* Character : CharacterStandIns)
	{
		if (Character && Character->bDeliveryTarget && Character->bOccupied
			&& NearestPlateSlot(PlateSlots, Character->GetActorLocation()) == PlateSlot)
		{
			return Character;
		}
	}
	return nullptr;
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
			if (PendingIngredientArrivalCounts.Contains(Visual->CellIndex))
			{
				// The board piece already exists logically, but its resting art should not
				// appear underneath the screen-space item that is still flying toward it.
				Visual->PieceIcon->SetVisibility(false);
				Visual->PieceMesh->SetVisibility(false);
				Visual->PieceLabel->SetVisibility(false);
			}
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
		if (!bUsingSceneAuthoredSeats)
		{
			for (ASDayCharacterStandIn* Character : CharacterStandIns)
			{
				if (Character && !Character->bSceneAuthoredSeat)
				{
					Character->Destroy();
				}
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
	LastPointerPosition = ScreenPosition;
	if (bPressed)
	{
		HandlePointerPressed(ScreenPosition);
		UpdateDraggedIcon(ScreenPosition);
	}
	else
	{
		HandlePointerReleased(ScreenPosition);
	}
}

void ASDayBoardPresenter::SimulatePointerMove(const FVector2D ScreenPosition)
{
	LastPointerPosition = ScreenPosition;
	UpdateDraggedIcon(ScreenPosition);
}

void ASDayBoardPresenter::SetUseExternalPointerDriver(const bool bEnabled)
{
	bUseExternalPointerDriver = bEnabled;
}

void ASDayBoardPresenter::CancelPointerInteraction()
{
	HideDragPreview();
	ASMergeBoard* Board = LogicBoard.Get();
	if (!Board)
	{
		Board = ASMergeBoard::FindBoard(this);
		LogicBoard = Board;
	}
	if (Board && Board->IsDragging())
	{
		Board->CancelPieceDrag(0);
		RefreshFromLogic();
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

#pragma region K2 moonyfli
namespace
{
	UAnimSequenceBase* LoadIngredientBinAnimation(const int32 BinIndex)
	{
		const FString AnimationPath = FString::Printf(
			TEXT("/Game/Day/Art/canguan/animation/box%d_Anim.box%d_Anim"),
			BinIndex + 1,
			BinIndex + 1);
		return LoadObject<UAnimSequenceBase>(nullptr, *AnimationPath);
	}

	// Speed is authored outside this class: the component PlayRate drives the single node
	// instance and the engine multiplies the sequence RateScale on top of it.
	float GetIngredientBinBaseRate(const USkeletalMeshComponent& AnimatedBox)
	{
		const float AuthoredRate = FMath::Abs(AnimatedBox.AnimationData.SavedPlayRate);
		return AuthoredRate > UE_KINDA_SMALL_NUMBER ? AuthoredRate : 1.0f;
	}

	/** Returns 0 when the effective rate cannot advance the sequence, meaning it never reaches an end. */
	float GetIngredientBinPlaySeconds(const USkeletalMeshComponent& AnimatedBox, const UAnimSequenceBase& Animation)
	{
		const float EffectiveRate = GetIngredientBinBaseRate(AnimatedBox) * FMath::Abs(Animation.RateScale);
		return EffectiveRate > UE_KINDA_SMALL_NUMBER ? Animation.GetPlayLength() / EffectiveRate : 0.0f;
	}
}

USkeletalMeshComponent* ASDayBoardPresenter::FindIngredientBinAnimComponent(const int32 BinIndex) const
{
	const UWorld* World = GetWorld();
	if (!World || BinIndex < 0)
	{
		return nullptr;
	}

#pragma region K2 moonyfli
	auto MatchesBinAnim = [BinIndex](const USkeletalMeshComponent* AnimatedBox) -> bool
	{
		if (!AnimatedBox)
		{
			return false;
		}
		const FString Name = AnimatedBox->GetName();
		const FString Exact = FString::Printf(TEXT("BoxAnim_%d"), BinIndex);
		// Level instances and Live Coding may keep the SCS suffix (_GEN_VARIABLE) or a _1 duplicate.
		return Name.Equals(Exact, ESearchCase::IgnoreCase)
			|| Name.StartsWith(Exact + TEXT("_"), ESearchCase::IgnoreCase);
	};

	USkeletalMeshComponent* Fallback = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate)
		{
			continue;
		}

		TInlineComponentArray<USkeletalMeshComponent*> AnimatedBoxes;
		Candidate->GetComponents(AnimatedBoxes);
		for (USkeletalMeshComponent* AnimatedBox : AnimatedBoxes)
		{
			if (!MatchesBinAnim(AnimatedBox))
			{
				continue;
			}
			if (Candidate->ActorHasTag(DayArtEnvironmentTag))
			{
				return AnimatedBox;
			}
			if (!Fallback)
			{
				Fallback = AnimatedBox;
			}
		}
	}
	return Fallback;
#pragma endregion K2 moonyfli
}

#pragma region K2 moonyfli
namespace
{
	bool MatchesAuthoredComponentName(const UActorComponent* Component, const FName TargetName)
	{
		if (!Component || TargetName.IsNone())
		{
			return false;
		}
		const FString Name = Component->GetName();
		const FString Exact = TargetName.ToString();
		return Name.Equals(Exact, ESearchCase::IgnoreCase)
			|| Name.StartsWith(Exact + TEXT("_"), ESearchCase::IgnoreCase);
	}
}

UStaticMeshComponent* ASDayBoardPresenter::FindGhostMeshComponent(AActor** OutOwner) const
{
	if (OutOwner)
	{
		*OutOwner = nullptr;
	}

	const UWorld* World = GetWorld();
	if (!World || GhostMeshComponentName.IsNone())
	{
		return nullptr;
	}

	UStaticMeshComponent* Fallback = nullptr;
	AActor* FallbackOwner = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate)
		{
			continue;
		}

		TInlineComponentArray<UStaticMeshComponent*> Meshes;
		Candidate->GetComponents(Meshes);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (!MatchesAuthoredComponentName(Mesh, GhostMeshComponentName))
			{
				continue;
			}
			if (Candidate->ActorHasTag(DayBoardPresentationPrivate::DayArtEnvironmentTag))
			{
				if (OutOwner)
				{
					*OutOwner = Candidate;
				}
				return Mesh;
			}
			if (!Fallback)
			{
				Fallback = Mesh;
				FallbackOwner = Candidate;
			}
		}
	}

	if (OutOwner)
	{
		*OutOwner = FallbackOwner;
	}
	return Fallback;
}

void ASDayBoardPresenter::EnsureGhostFireVfx()
{
	if (!bEnableGhostFireVfx || GhostFireAnchors.Num() == 0)
	{
		return;
	}

	AActor* Restaurant = nullptr;
	UStaticMeshComponent* GhostMesh = FindGhostMeshComponent(&Restaurant);
	if (!Restaurant || !GhostMesh)
	{
		return;
	}

	DayBoardPresentationPrivate::FDayCameraFrame Frame;
	const bool bHaveFrame = DayBoardPresentationPrivate::TryGetDayCameraFrame(GetWorld(), Frame);

	float MinRight = 0.0f;
	float MaxRight = 0.0f;
	float MinUp = 0.0f;
	float MaxUp = 0.0f;
	float CenterDepth = 0.0f;
	if (bHaveFrame)
	{
		const FVector Origin = GhostMesh->Bounds.Origin;
		const FVector Extent = GhostMesh->Bounds.BoxExtent;
		bool bHasCorner = false;
		for (int32 SignX = -1; SignX <= 1; SignX += 2)
		{
			for (int32 SignY = -1; SignY <= 1; SignY += 2)
			{
				for (int32 SignZ = -1; SignZ <= 1; SignZ += 2)
				{
					const FVector Corner = Origin + FVector(
						static_cast<double>(SignX) * Extent.X,
						static_cast<double>(SignY) * Extent.Y,
						static_cast<double>(SignZ) * Extent.Z);
					const FVector InFrame = Frame.ToFrame(Corner);
					if (!bHasCorner)
					{
						MinRight = MaxRight = InFrame.X;
						MinUp = MaxUp = InFrame.Y;
						bHasCorner = true;
					}
					else
					{
						MinRight = FMath::Min(MinRight, InFrame.X);
						MaxRight = FMath::Max(MaxRight, InFrame.X);
						MinUp = FMath::Min(MinUp, InFrame.Y);
						MaxUp = FMath::Max(MaxUp, InFrame.Y);
					}
				}
			}
		}
		CenterDepth = Frame.ToFrame(Origin).Z;
	}

	TInlineComponentArray<UNiagaraComponent*> ExistingFires;
	Restaurant->GetComponents(ExistingFires);

	for (const FSDayGhostFireAnchor& Anchor : GhostFireAnchors)
	{
		UNiagaraSystem* System = Anchor.System.LoadSynchronous();
		if (!System)
		{
			continue;
		}

		FVector WorldLocation = GhostMesh->GetComponentLocation();
		if (bAutoPlaceGhostFire && bHaveFrame)
		{
			const float AlphaX = FMath::Clamp(0.5f + 0.5f * Anchor.FrameBias.X, 0.0f, 1.0f);
			const float AlphaY = FMath::Clamp(0.5f + 0.5f * Anchor.FrameBias.Y, 0.0f, 1.0f);
			WorldLocation = Frame.ToWorld(FVector(
				FMath::Lerp(MinRight, MaxRight, AlphaX),
				FMath::Lerp(MinUp, MaxUp, AlphaY),
				CenterDepth - GhostFireTowardCameraCm));
		}

		UNiagaraComponent* Fire = nullptr;
		for (UNiagaraComponent* Candidate : ExistingFires)
		{
			if (MatchesAuthoredComponentName(Candidate, Anchor.ComponentName))
			{
				Fire = Candidate;
				break;
			}
		}

		if (!Fire)
		{
			if (!bAutoPlaceGhostFire)
			{
				continue;
			}
			Fire = NewObject<UNiagaraComponent>(Restaurant, Anchor.ComponentName);
			if (!Fire)
			{
				continue;
			}
			Fire->SetAsset(System);
			Fire->SetupAttachment(GhostMesh);
			Fire->SetAutoActivate(true);
			Fire->RegisterComponent();
			Restaurant->AddInstanceComponent(Fire);
			Fire->SetAbsolute(false, false, true);
			Fire->SetWorldScale3D(GhostFireWorldScale);
			Fire->SetWorldLocation(WorldLocation);
		}
		else if (!Fire->GetAsset())
		{
			Fire->SetAsset(System);
		}

		Fire->Activate(true);
	}
}
#pragma endregion K2 moonyfli

void ASDayBoardPresenter::PlayIngredientSpawnFeedback(
	ASDayIngredientBinVisual* Bin,
	const FName IngredientId,
	const int32 SpawnedCellIndex)
{
	ASDayCellVisual* TargetCell = GetCellVisual(SpawnedCellIndex);
	UWorld* World = GetWorld();
	if (!Bin || !TargetCell || !World)
	{
		return;
	}

	const FVector Start = Bin->GetActorTransform().TransformPosition(IngredientBinVfxStartOffset)
		+ IngredientBinVfxViewOffset;
	const FVector End = (TargetCell->PieceIcon
		? TargetCell->PieceIcon->GetComponentLocation()
		: TargetCell->GetActorTransform().TransformPosition(IngredientBinVfxTargetOffset))
		+ IngredientBinVfxViewOffset;
	UNiagaraSystem* FogSystem = IngredientBinFogSystem.LoadSynchronous();
	UNiagaraSystem* BurstSystem = IngredientBinFoodBurstSystem.LoadSynchronous();
	UNiagaraSystem* TrailSystem = IngredientBinTrailSystem.LoadSynchronous();
	if (FogSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, FogSystem, Start, FRotator::ZeroRotator, IngredientBinFogScale);
	}
	if (BurstSystem && bSpawnFoodBurstAtBin)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, BurstSystem, Start, FRotator::ZeroRotator, IngredientBinFoodBurstScale);
	}
	if (TrailSystem)
	{
		// The Niagara graph writes the absolute User Start/Target Position values
		// directly to Particles.Position. Spawning this component at Start as well
		// applies the bin translation twice for local-space emitters and sends the
		// effect off screen. Keep the component transform neutral so the exposed
		// Position parameters are the single source of truth for the rendered flight path.
		if (UNiagaraComponent* Trail = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			TrailSystem,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			IngredientBinTrailScale,
			true,
			false,
			ENCPoolMethod::None,
			false))
		{
			if (!IngredientTrailStartParameter.IsNone())
			{
				Trail->SetVariablePosition(IngredientTrailStartParameter, Start);
			}
			if (!IngredientTrailTargetParameter.IsNone())
			{
				Trail->SetVariablePosition(IngredientTrailTargetParameter, End);
			}
			if (!IngredientTrailDurationParameter.IsNone())
			{
				Trail->SetVariableFloat(
					IngredientTrailDurationParameter,
					FMath::Max(0.01f, IngredientTrailDuration));
			}
			FBox TrailBounds(EForceInit::ForceInit);
			TrailBounds += Start;
			TrailBounds += End;
			Trail->SetSystemFixedBounds(
				TrailBounds.ExpandBy(FMath::Max(0.0f, IngredientTrailBoundsPadding)));
			// Position-typed user parameters must be in place before the first spawn/update tick.
			Trail->Activate(true);
		}
	}

	UTexture2D* IconTexture = DayBoardPresentationPrivate::ResolveIngredientIcon(IngredientId);
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!IconTexture || !PlayerController)
	{
		return;
	}

	FVector2D StartScreen;
	FVector2D EndScreen;
	if (!UGameplayStatics::ProjectWorldToScreen(PlayerController, Start, StartScreen)
		|| !UGameplayStatics::ProjectWorldToScreen(PlayerController, End, EndScreen))
	{
		return;
	}

	USDayDragPreview* FlyingPreview = CreateWidget<USDayDragPreview>(
		PlayerController,
		USDayDragPreview::StaticClass());
	if (!FlyingPreview)
	{
		return;
	}
	PendingIngredientArrivalCounts.FindOrAdd(SpawnedCellIndex)++;
	FlyingPreview->AddToViewport(160);
	const float PreviewWidth = FMath::Max(1.0f, IngredientFlyIconWidth);
	const float Aspect = IconTexture->GetSizeX() > 0
		? static_cast<float>(IconTexture->GetSizeY()) / static_cast<float>(IconTexture->GetSizeX())
		: 1.0f;
	FlyingPreview->ShowPreview(
		IconTexture,
		StartScreen,
		FVector2D(PreviewWidth, PreviewWidth * FMath::Clamp(Aspect, 0.65f, 1.35f)));

	const double StartSeconds = World->GetTimeSeconds();
	const float DurationSeconds = FMath::Max(0.01f, IngredientFlyDuration);
	const float FlyArcHeight = FMath::Max(0.0f, IngredientFlyArcHeight);
	const bool bBurstAtTarget = bSpawnFoodBurstAtTarget;
	const FVector BurstScale = IngredientBinFoodBurstScale;
	const uint64 FlightId = ++NextIngredientFlightId;
	FTimerHandle& FlightTimer = IngredientFlightTimers.Add(FlightId);
	const TWeakObjectPtr<USDayDragPreview> WeakFlyingPreview(FlyingPreview);
	const TWeakObjectPtr<ASDayBoardPresenter> WeakPresenter(this);
	World->GetTimerManager().SetTimer(
		FlightTimer,
		FTimerDelegate::CreateWeakLambda(this, [WeakPresenter, WeakFlyingPreview, FlightId, StartScreen, EndScreen, End, StartSeconds, DurationSeconds, FlyArcHeight, BurstSystem, bBurstAtTarget, BurstScale, SpawnedCellIndex]()
		{
			ASDayBoardPresenter* Presenter = WeakPresenter.Get();
			USDayDragPreview* Preview = WeakFlyingPreview.Get();
			if (!Presenter || !Presenter->GetWorld())
			{
				return;
			}
			UWorld* CurrentWorld = Presenter->GetWorld();
			const float Alpha = FMath::Clamp(
				static_cast<float>((CurrentWorld->GetTimeSeconds() - StartSeconds) / DurationSeconds),
				0.0f,
				1.0f);
			const float EasedAlpha = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.2f);
			FVector2D Position = FMath::Lerp(StartScreen, EndScreen, EasedAlpha);
			Position.Y -= FMath::Sin(Alpha * PI) * FlyArcHeight;
			if (Preview)
			{
				Preview->MovePreview(Position);
			}
			if (Alpha >= 1.0f)
			{
				// Copy the presenter-owned handle before removing it. Clearing an executing timer
				// destroys its delegate immediately, so all captured state must be finished first.
				FTimerHandle TimerHandleToClear = Presenter->IngredientFlightTimers.FindRef(FlightId);
				if (BurstSystem && bBurstAtTarget)
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(
						CurrentWorld, BurstSystem, End, FRotator::ZeroRotator, BurstScale);
				}
				if (Preview)
				{
					Preview->HidePreview();
					Preview->RemoveFromParent();
				}

				if (int32* PendingCount = Presenter->PendingIngredientArrivalCounts.Find(SpawnedCellIndex))
				{
					if (--(*PendingCount) <= 0)
					{
						Presenter->PendingIngredientArrivalCounts.Remove(SpawnedCellIndex);
						if (ASDayCellVisual* ArrivedCell = Presenter->GetCellVisual(SpawnedCellIndex))
						{
							ArrivedCell->RefreshVisual();
						}
					}
				}

				Presenter->IngredientFlightTimers.Remove(FlightId);
				CurrentWorld->GetTimerManager().ClearTimer(TimerHandleToClear);
			}
		}),
		1.0f / 60.0f,
		true);
}

void ASDayBoardPresenter::PlayIngredientBinAnimation(const int32 BinIndex)
{
	if (BinIndex < 0 || BinIndex >= DayIngredientBinCount)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UAnimSequenceBase* Animation = LoadIngredientBinAnimation(BinIndex);
	USkeletalMeshComponent* AnimatedBox = FindIngredientBinAnimComponent(BinIndex);
	if (!Animation || !AnimatedBox)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Ingredient bin animation skipped: Anim=%s BoxAnim_%d=%s"),
			Animation ? *Animation->GetName() : TEXT("null"),
			BinIndex,
			AnimatedBox ? *AnimatedBox->GetName() : TEXT("null"));
		return;
	}

	if (FTimerHandle* PendingTimer = BinAnimTimers.Find(BinIndex))
	{
		World->GetTimerManager().ClearTimer(*PendingTimer);
	}

	AnimatedBox->SetAnimation(Animation);
	AnimatedBox->SetPlayRate(GetIngredientBinBaseRate(*AnimatedBox));
	AnimatedBox->SetPosition(0.0f, false);
	AnimatedBox->Play(false);

	const float OpenSeconds = GetIngredientBinPlaySeconds(*AnimatedBox, *Animation);
	if (!bBinAnimAutoClose || OpenSeconds <= 0.0f)
	{
		return;
	}

	const float CloseDelay = FMath::Max(OpenSeconds + BinAnimHoldSeconds, UE_KINDA_SMALL_NUMBER);
	FTimerDelegate CloseDelegate = FTimerDelegate::CreateUObject(this, &ASDayBoardPresenter::CloseIngredientBinAnimation, BinIndex);
	World->GetTimerManager().SetTimer(BinAnimTimers.FindOrAdd(BinIndex), CloseDelegate, CloseDelay, false);
}

void ASDayBoardPresenter::CloseIngredientBinAnimation(const int32 BinIndex)
{
	UWorld* World = GetWorld();
	USkeletalMeshComponent* AnimatedBox = FindIngredientBinAnimComponent(BinIndex);
	if (!World || !AnimatedBox)
	{
		return;
	}

	const UAnimSequenceBase* Animation = LoadIngredientBinAnimation(BinIndex);
	if (!Animation)
	{
		return;
	}

	// A negative play rate rewinds the single-node instance, so the lid closes with the same art.
	AnimatedBox->SetPosition(Animation->GetPlayLength(), false);
	AnimatedBox->SetPlayRate(-GetIngredientBinBaseRate(*AnimatedBox));
	AnimatedBox->Play(false);

	const float CloseSeconds = FMath::Max(GetIngredientBinPlaySeconds(*AnimatedBox, *Animation), UE_KINDA_SMALL_NUMBER);
	FTimerDelegate RestDelegate = FTimerDelegate::CreateUObject(this, &ASDayBoardPresenter::RestIngredientBinAnimation, BinIndex);
	World->GetTimerManager().SetTimer(BinAnimTimers.FindOrAdd(BinIndex), RestDelegate, CloseSeconds, false);
}

void ASDayBoardPresenter::RestIngredientBinAnimation(const int32 BinIndex)
{
	if (USkeletalMeshComponent* AnimatedBox = FindIngredientBinAnimComponent(BinIndex))
	{
		AnimatedBox->Stop();
		AnimatedBox->SetPlayRate(GetIngredientBinBaseRate(*AnimatedBox));
		AnimatedBox->SetPosition(0.0f, false);
	}
	BinAnimTimers.Remove(BinIndex);
}
#pragma endregion K2 moonyfli

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

void ASDayBoardPresenter::UpdateDraggedIcon(const FVector2D& ScreenPosition)
{
	ASMergeBoard* Board = LogicBoard.Get();
	if (!Board)
	{
		Board = ASMergeBoard::FindBoard(this);
		LogicBoard = Board;
	}
	if (!Board || !Board->IsDragging())
	{
		HideDragPreview();
		return;
	}

	const int32 CellIndex = Board->GetActiveDragCellIndex();
	ASDayCellVisual* Cell = GetCellVisual(CellIndex);
	if (!Cell)
	{
		HideDragPreview();
		return;
	}

	FSDishPiece Piece;
	if (!Board->TryGetPiece(CellIndex, Piece))
	{
		HideDragPreview();
		return;
	}

	const USDayBoardVisualConfig* Config = VisualConfig.LoadSynchronous();
	UTexture2D* Texture = DayBoardPresentationPrivate::ResolveDishIcon(
		Config,
		Piece.IngredientId,
		Piece.Level);
	if (!Texture)
	{
		HideDragPreview();
		return;
	}

	EnsureDragPreview();
	if (!DragPreview)
	{
		return;
	}

	if (!bDragIconTuneResolved)
	{
		DragIconTune = DayBoardPresentationPrivate::ResolveDishIconTune(Config);
		bDragIconTuneResolved = true;
	}

	const float PreviewSize = DragIconTune.WorldSize
		* (1.0f + DragIconTune.ScalePerLevel * static_cast<float>(Piece.Level))
		* DragIconTune.SelectedScale
		* DragIconTune.DragPreviewScale;
	DragPreview->ShowPreview(
		Texture,
		ScreenPosition,
		FVector2D(PreviewSize, PreviewSize));

	// The cell refresh still owns the resting icon. Hide it only while the pointer preview is live,
	// so click-release-click selection continues to show the selected dish between clicks.
	if (Cell->PieceIcon)
	{
		Cell->PieceIcon->SetVisibility(false);
	}
}

void ASDayBoardPresenter::EnsureDragPreview()
{
	if (IsValid(DragPreview))
	{
		// Day startup replaces pre-existing widgets before installing the formal HUD.
		if (!DragPreview->IsInViewport())
		{
			DragPreview->AddToViewport(200);
		}
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		return;
	}

	DragPreview = CreateWidget<USDayDragPreview>(
		PlayerController,
		USDayDragPreview::StaticClass());
	if (DragPreview)
	{
		// Day HUD uses 80 and the debug overlays use at most 120.
		DragPreview->AddToViewport(200);
		DragPreview->HidePreview();
	}
}

void ASDayBoardPresenter::HideDragPreview()
{
	if (DragPreview)
	{
		DragPreview->HidePreview();
	}
}

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

bool ASDayBoardPresenter::TryDropPieceAndNotify(
	ASMergeBoard* Board,
	const int32 FromCellIndex,
	const int32 ToCellIndex)
{
	if (!Board)
	{
		return false;
	}

	FSDishPiece FromPiece;
	FSDishPiece ToPiece;
	const bool bMergeCandidate = Board->TryGetPiece(FromCellIndex, FromPiece)
		&& Board->TryGetPiece(ToCellIndex, ToPiece)
		&& FromPiece.IngredientId == ToPiece.IngredientId
		&& FromPiece.Level == ToPiece.Level;
	const bool bDropSucceeded = Board->TryDropPiece(FromCellIndex, ToCellIndex);
	if (bDropSucceeded)
	{
		// A piece can be moved after its flight overlay has finished but before a stale
		// arrival flag was released. Never let that old cell index hide future contents.
		PendingIngredientArrivalCounts.Remove(FromCellIndex);
		if (bMergeCandidate)
		{
			PendingIngredientArrivalCounts.Remove(ToCellIndex);
			BP_OnIngredientMergeCompleted(ToPiece.IngredientId, ToPiece.Level + 1, ToCellIndex);
		}
	}
	return bDropSucceeded;
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

	// Resolve the world trace once. Cells deliberately win over delivery proxies, so a
	// plate/portrait target can never steal a merge-cell interaction.
	AActor* HitActor = HitTest(ScreenPosition);
	if (ASDayCellVisual* Cell = Cast<ASDayCellVisual>(HitActor))
	{
		if (Board->IsDragging())
		{
			const int32 FromIndex = Board->GetActiveDragCellIndex();
			if (Cell->CellIndex != FromIndex)
			{
				// Second click completes the click-release-click interaction immediately.
				TryDropPieceAndNotify(Board, FromIndex, Cell->CellIndex);
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
		return;
	}

	if (ASDayIngredientBinVisual* Bin = Cast<ASDayIngredientBinVisual>(HitActor))
	{
#pragma region K2 moonyfli
		int32 SpawnedCellIndex = INDEX_NONE;
		const bool bSpawnSucceeded = Board->TrySpawnFromMotherPieceWithResult(
			Bin->IngredientId,
			SpawnedCellIndex);
		BP_OnIngredientBinClicked(Bin->BinIndex, Bin->IngredientId, bSpawnSucceeded);
		if (bSpawnSucceeded)
		{
			PlayIngredientBinAnimation(Bin->BinIndex);
			PlayIngredientSpawnFeedback(Bin, Bin->IngredientId, SpawnedCellIndex);
		}
#pragma endregion K2 moonyfli
		RefreshFromLogic();
		return;
	}

	ASDayCharacterStandIn* Character = Cast<ASDayCharacterStandIn>(HitActor);
	if (!Character)
	{
		Character = FindDeliveryPlateTarget(ScreenPosition);
	}
	if (Character)
	{
		// Second click of the click-release-click flow: the portrait or its plate slot delivers.
		if (TryDeliverToCharacter(Character, Board))
		{
			bDropHandledOnPress = true;
		}
		RefreshFromLogic();
		return;
	}
}

void ASDayBoardPresenter::HandlePointerReleased(const FVector2D& ScreenPosition)
{
	HideDragPreview();
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
	if (ASDayCellVisual* Cell = Cast<ASDayCellVisual>(HitActor))
	{
		if (Cell->CellIndex != FromIndex)
		{
			// Holding on A and releasing over B completes the drag interaction.
			TryDropPieceAndNotify(Board, FromIndex, Cell->CellIndex);
		}
		RefreshFromLogic();
		return;
	}

	ASDayCharacterStandIn* Character = Cast<ASDayCharacterStandIn>(HitActor);
	if (!Character)
	{
		Character = FindDeliveryPlateTarget(ScreenPosition);
	}
	if (Character)
	{
		// The projected plate quarter and the portrait both deliver to the customer in that slot.
		TryDeliverToCharacter(Character, Board);
		RefreshFromLogic();
		return;
	}
	// Releasing a simple click over A (or outside the board) keeps A selected,
	// allowing the next click on B to move or merge it.
	RefreshFromLogic();
}

TSharedRef<SWidget> USDayHUD::RebuildWidget()
{
#pragma region K2 moonyfli
	// WBP_SDayHUD is a thin wrapper. If the designer tree has no C++ HUD root, replace it so the
	// chrome toggle and debug controls actually exist at runtime.
	if (WidgetTree && (!WidgetTree->RootWidget || WidgetTree->FindWidget(TEXT("DayHUDRoot")) == nullptr))
	{
		BuildWidgetTree();
	}
#pragma endregion K2 moonyfli
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
	if (SettlementWidget)
	{
		SettlementWidget->RemoveFromParent();
		SettlementWidget = nullptr;
		RestoreDayInputMode();
	}
	if (RestaurantEndDialogueWidget)
	{
		RestaurantEndDialogueWidget->RemoveFromParent();
		RestaurantEndDialogueWidget = nullptr;
		RestoreDayInputMode();
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

#pragma region K2 moonyfli
	UHorizontalBox* ToggleRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("DayHudChromeRow"));
	ToggleRow->SetVisibility(ESlateVisibility::Collapsed);
	Root->AddChildToVerticalBox(ToggleRow)->SetPadding(FMargin(8.0f, 6.0f, 8.0f, 0.0f));

	USizeBox* ToggleSpacer = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("DayHudChromeSpacer"));
	ToggleSpacer->SetVisibility(ESlateVisibility::HitTestInvisible);
	ToggleRow->AddChildToHorizontalBox(ToggleSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Tiny, almost invisible hit target in the top-right. It stays after the HUD chrome is hidden.
	USizeBox* ToggleBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("DayHudChromeSize"));
	ToggleBox->SetWidthOverride(28.0f);
	ToggleBox->SetHeightOverride(28.0f);
	ToggleRow->AddChildToHorizontalBox(ToggleBox);

	ChromeToggle = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("DayHudChromeSwitch"));
	ChromeToggle->SetIsChecked(true);
	ChromeToggle->SetToolTipText(FText::FromString(TEXT("显示/隐藏运行时控制信息和按钮")));
	FCheckBoxStyle ToggleStyle = ChromeToggle->GetWidgetStyle();
	const FLinearColor HiddenTint(1.0f, 1.0f, 1.0f, 0.0f);
	ToggleStyle.UncheckedImage.TintColor = FSlateColor(HiddenTint);
	ToggleStyle.UncheckedHoveredImage.TintColor = FSlateColor(HiddenTint);
	ToggleStyle.UncheckedPressedImage.TintColor = FSlateColor(HiddenTint);
	ToggleStyle.CheckedImage.TintColor = FSlateColor(HiddenTint);
	ToggleStyle.CheckedHoveredImage.TintColor = FSlateColor(HiddenTint);
	ToggleStyle.CheckedPressedImage.TintColor = FSlateColor(HiddenTint);
	ToggleStyle.UndeterminedImage.TintColor = FSlateColor(HiddenTint);
	ToggleStyle.UndeterminedHoveredImage.TintColor = FSlateColor(HiddenTint);
	ToggleStyle.UndeterminedPressedImage.TintColor = FSlateColor(HiddenTint);
	ChromeToggle->SetWidgetStyle(ToggleStyle);
	ToggleBox->AddChild(ChromeToggle);
	ChromeToggle->OnCheckStateChanged.AddDynamic(this, &USDayHUD::HandleChromeVisibilityChanged);

	ControlsHost = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DayHUDControls"));
	// The generated HUD chrome is a runtime debug/control surface. Keep it disabled
	// in every configuration; the foreground art HUD remains available.
	ControlsHost->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* ControlsSlot = Root->AddChildToVerticalBox(ControlsHost);
	ControlsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
#pragma endregion K2 moonyfli

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
	auto MakeIngredientButton = [this, &MakeText](const TCHAR* Name, const TCHAR* Label, const FName IngredientId)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("%s_Content"), Name));
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			*FString::Printf(TEXT("%s_Icon"), Name));
		if (UTexture2D* Texture = ResolveIngredientIcon(IngredientId))
		{
			Icon->SetBrushFromTexture(Texture, false);
		}
		Icon->SetDesiredSizeOverride(FVector2D(46.0f, 40.0f));
		Content->AddChildToHorizontalBox(Icon)->SetPadding(FMargin(2.0f, 1.0f, 5.0f, 1.0f));

		UTextBlock* Text = MakeText(*FString::Printf(TEXT("%s_Label"), Name), 20, FLinearColor::White);
		Text->SetText(FText::FromString(Label));
		Text->SetJustification(ETextJustify::Center);
		Content->AddChildToHorizontalBox(Text)->SetVerticalAlignment(VAlign_Center);
		Button->AddChild(Content);
		return Button;
	};
	PhaseText = MakeText(TEXT("PhaseText"), 24, FLinearColor(0.10f, 0.95f, 0.75f));
	ControlsHost->AddChildToVerticalBox(PhaseText)->SetPadding(FMargin(18.0f, 12.0f, 18.0f, 4.0f));

	OrderText = MakeText(TEXT("DayOrderText"), 20, FLinearColor(0.98f, 0.90f, 0.42f));
	ControlsHost->AddChildToVerticalBox(OrderText)->SetPadding(FMargin(18.0f, 2.0f, 18.0f, 4.0f));

	UHorizontalBox* Orders = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("OrderRow"));
	ControlsHost->AddChildToVerticalBox(Orders)->SetPadding(FMargin(12.0f, 4.0f));
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
	UVerticalBoxSlot* SpacerSlot = ControlsHost->AddChildToVerticalBox(WorldSpacer);
	SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	FeedbackText = MakeText(TEXT("DayFeedbackText"), 20, FLinearColor(1.0f, 0.82f, 0.20f));
	ControlsHost->AddChildToVerticalBox(FeedbackText)->SetPadding(FMargin(18.0f, 4.0f));

	InventoryText = MakeText(TEXT("DayInventoryText"), 20, FLinearColor::White);
	ControlsHost->AddChildToVerticalBox(InventoryText)->SetPadding(FMargin(18.0f, 4.0f));

	UWrapBox* Ingredients = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("IngredientRow"));
	Ingredients->SetInnerSlotPadding(FVector2D(4.0f, 4.0f));
	ControlsHost->AddChildToVerticalBox(Ingredients)->SetPadding(FMargin(12.0f, 2.0f));
	UButton* LingGu = MakeIngredientButton(TEXT("DayLingGuButton"), TEXT("灵谷"), DayLingGuId);
	UButton* Yin = MakeIngredientButton(TEXT("DayYinShanJunButton"), TEXT("阴山菌"), DayYinShanJunId);
	UButton* Chi = MakeIngredientButton(TEXT("DayChiYanJiaoButton"), TEXT("赤焰椒"), DayChiYanJiaoId);
	UButton* Yue = MakeIngredientButton(TEXT("DayYueLinYuButton"), TEXT("月鳞鱼"), DayYueLinYuId);
	UButton* Xuan = MakeIngredientButton(TEXT("DayXuanYuQinButton"), TEXT("玄羽禽"), DayXuanYuQinId);
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
	ControlsHost->AddChildToVerticalBox(GiftTabText)->SetPadding(FMargin(18.0f, 4.0f));

	UHorizontalBox* Flow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FlowRow"));
	ControlsHost->AddChildToVerticalBox(Flow)->SetPadding(FMargin(12.0f, 2.0f, 12.0f, 10.0f));
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
	RefreshSettlement(*GameInstance);
	RefreshRestaurantEndDialogue(*GameInstance);

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
	case ESGamePhase::DaySettlement:
		FlowLabel = GameInstance->GetPendingDaySettlement().Outcome == ESDaySettlementOutcome::Success
			? TEXT("确认成功结算")
			: TEXT("确认回档重开");
		break;
	case ESGamePhase::Ending: FlowLabel = TEXT("尾声"); break;
	default: break;
	}
	if (FlowButton)
	{
		FlowButton->SetVisibility(FlowLabel.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		SetButtonText(FlowButton, FlowLabel);
	}

	RefreshForegroundReadouts(*GameInstance); //add by K2
}

void USDayHUD::RefreshSettlement(const USChefGameInstance& GameInstance)
{
	const bool bShouldShow = GameInstance.HasPendingDaySettlement();
	if (!bShouldShow)
	{
		if (SettlementWidget)
		{
			SettlementWidget->RemoveFromParent();
			SettlementWidget = nullptr;
			RestoreDayInputMode();
		}
		return;
	}

	if (SettlementWidget)
	{
		return;
	}

	if (!SettlementWidgetClass)
	{
		SettlementWidgetClass = LoadClass<USDaySettlementWidget>(
			nullptr,
			TEXT("/Game/Day/UI/Settlement/WBP_DaySettlement.WBP_DaySettlement_C"));
	}
	if (!SettlementWidgetClass)
	{
		if (!bSettlementClassWarningLogged)
		{
			bSettlementClassWarningLogged = true;
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[DaySettlement] Assign SettlementWidgetClass on WBP_SDayHUD or create /Game/Day/UI/Settlement/WBP_DaySettlement."));
		}
		return;
	}

	SettlementWidget = CreateWidget<USDaySettlementWidget>(GetOwningPlayer(), SettlementWidgetClass);
	if (!SettlementWidget)
	{
		return;
	}

	SettlementWidget->AddToViewport(200);
	SettlementWidget->PresentSettlement(GameInstance.GetPendingDaySettlement());
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(SettlementWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
	}
}

void USDayHUD::RefreshRestaurantEndDialogue(const USChefGameInstance& GameInstance)
{
	const bool bShouldShow = GameInstance.IsAwaitingRestaurantEndDialogue();
	if (!bShouldShow)
	{
		if (RestaurantEndDialogueWidget)
		{
			RestaurantEndDialogueWidget->RemoveFromParent();
			RestaurantEndDialogueWidget = nullptr;
			RestoreDayInputMode();
		}
		return;
	}

	if (RestaurantEndDialogueWidget)
	{
		return;
	}

	if (!RestaurantEndDialogueWidgetClass)
	{
		RestaurantEndDialogueWidgetClass = LoadClass<USRestaurantEndDialogueWidget>(
			nullptr,
			TEXT("/Game/Day/UI/Dialogue/WBP_RestaurantEndDialogue.WBP_RestaurantEndDialogue_C"));
	}
	if (!RestaurantEndDialogueWidgetClass)
	{
		if (!bRestaurantDialogueClassWarningLogged)
		{
			bRestaurantDialogueClassWarningLogged = true;
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[RestaurantDialogue] Assign RestaurantEndDialogueWidgetClass on WBP_SDayHUD or create /Game/Day/UI/Dialogue/WBP_RestaurantEndDialogue."));
		}
		return;
	}

	RestaurantEndDialogueWidget = CreateWidget<USRestaurantEndDialogueWidget>(
		GetOwningPlayer(),
		RestaurantEndDialogueWidgetClass);
	if (!RestaurantEndDialogueWidget)
	{
		return;
	}

	RestaurantEndDialogueWidget->AddToViewport(210);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(RestaurantEndDialogueWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
	}
}

void USDayHUD::RestoreDayInputMode()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->bShowMouseCursor = true;
	}
}

#pragma region K2 moonyfli
void USDayHUD::ResolveForegroundReadouts()
{
	CoinAmountText.Reset();
	RevenueCurrentText.Reset();
	RevenueTargetText.Reset();
	IngredientCountLingGuText.Reset();
	IngredientCountYinShanJunText.Reset();
	IngredientCountChiYanJiaoText.Reset();
	IngredientCountYueLinYuText.Reset();
	IngredientCountXuanYuQinText.Reset();
	BusinessTimeRemainingText.Reset();
	RevenueFlyTargetWidget.Reset();
	IngredientBinCountLingGuText.Reset();
	IngredientBinCountYinShanJunText.Reset();
	IngredientBinCountChiYanJiaoText.Reset();
	IngredientBinCountYueLinYuText.Reset();
	IngredientBinCountXuanYuQinText.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	auto CacheReadouts = [this](UUserWidget* Page)
	{
		if (!Page)
		{
			return false;
		}

		UTextBlock* Coin = Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("CoinAmount")));
		UTextBlock* Current = Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("RevenueCurrent")));
		UTextBlock* Target = Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("RevenueTarget")));
		UTextBlock* LingGuCount = Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("IngredientCount_LingGu")));
		UTextBlock* YinShanJunCount = Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("IngredientCount_YinShanJun")));
		UTextBlock* ChiYanJiaoCount = Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("IngredientCount_ChiYanJiao")));
		UTextBlock* YueLinYuCount = Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("IngredientCount_YueLinYu")));
		UTextBlock* XuanYuQinCount = Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("IngredientCount_XuanYuQin")));
		UTextBlock* BusinessTime = Cast<UTextBlock>(Page->GetWidgetFromName(TEXT("BusinessTimeRemaining")));
		UWidget* FlyTarget = Page->GetWidgetFromName(RevenueFlyTargetWidgetName);
		if (!Coin
			&& !Current
			&& !Target
			&& !LingGuCount
			&& !YinShanJunCount
			&& !ChiYanJiaoCount
			&& !YueLinYuCount
			&& !XuanYuQinCount
			&& !BusinessTime
			&& !FlyTarget)
		{
			return false;
		}

		CoinAmountText = Coin;
		RevenueCurrentText = Current;
		RevenueTargetText = Target;
		IngredientCountLingGuText = LingGuCount;
		IngredientCountYinShanJunText = YinShanJunCount;
		IngredientCountChiYanJiaoText = ChiYanJiaoCount;
		IngredientCountYueLinYuText = YueLinYuCount;
		IngredientCountXuanYuQinText = XuanYuQinCount;
		BusinessTimeRemainingText = BusinessTime;
		RevenueFlyTargetWidget = FlyTarget;
		return true;
	};

	// The foreground may be created by the level blueprint and added directly to the viewport.
	// Prefer that top-level instance before falling back to the legacy camera widget component.
	TArray<UUserWidget*> ViewportWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
		this,
		ViewportWidgets,
		UUserWidget::StaticClass(),
		true);
	bool bResolvedForegroundPage = false;
	for (UUserWidget* ViewportWidget : ViewportWidgets)
	{
		if (CacheReadouts(ViewportWidget))
		{
			bResolvedForegroundPage = true;
			break;
		}
	}

	if (!bResolvedForegroundPage)
	{
		TInlineComponentArray<UWidgetComponent*> WidgetComponents;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			It->GetComponents(WidgetComponents);
			for (const UWidgetComponent* WidgetComponent : WidgetComponents)
			{
				if (!WidgetComponent || !WidgetComponent->ComponentHasTag(DayArtForegroundTag))
				{
					continue;
				}
				if (CacheReadouts(WidgetComponent->GetUserWidgetObject()))
				{
					bResolvedForegroundPage = true;
					break;
				}
			}
			if (bResolvedForegroundPage)
			{
				break;
			}
		}
	}

	// The box labels are world components, so their placement follows the authored restaurant
	// composition instead of a phone-resolution-dependent foreground canvas. The component names
	// are the only C++/Blueprint contract; their parent Box, transform, font and material stay in BP.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UTextRenderComponent*> TextComponents;
		It->GetComponents(TextComponents);
		for (UTextRenderComponent* TextComponent : TextComponents)
		{
			if (!TextComponent)
			{
				continue;
			}

			const FName ComponentName = TextComponent->GetFName();
			if (ComponentName == TEXT("IngredientCount_LingGu"))
			{
				IngredientBinCountLingGuText = TextComponent;
			}
			else if (ComponentName == TEXT("IngredientCount_YinShanJun"))
			{
				IngredientBinCountYinShanJunText = TextComponent;
			}
			else if (ComponentName == TEXT("IngredientCount_ChiYanJiao"))
			{
				IngredientBinCountChiYanJiaoText = TextComponent;
			}
			else if (ComponentName == TEXT("IngredientCount_YueLinYu"))
			{
				IngredientBinCountYueLinYuText = TextComponent;
			}
			else if (ComponentName == TEXT("IngredientCount_XuanYuQin"))
			{
				IngredientBinCountXuanYuQinText = TextComponent;
			}
		}
	}
}

bool USDayHUD::ResolveRevenueFlyTargetPosition(FVector2D& OutPixelPosition)
{
	if (!RevenueFlyTargetWidget.IsValid())
	{
		ResolveForegroundReadouts();
	}

	UWidget* TargetWidget = RevenueFlyTargetWidget.Get();
	if (!TargetWidget)
	{
		// Keep old foreground assets functional while the named designer anchor is being added.
		TargetWidget = RevenueCurrentText.Get();
		if (TargetWidget && !bRevenueFlyTargetFallbackWarningLogged)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[DayRevenueFly] Widget '%s' was not found; using RevenueCurrent center until the UMG anchor is configured."),
				*RevenueFlyTargetWidgetName.ToString());
			bRevenueFlyTargetFallbackWarningLogged = true;
		}
	}
	if (!TargetWidget)
	{
		return false;
	}

	const FGeometry& Geometry = TargetWidget->GetCachedGeometry();
	const FVector2D LocalSize = Geometry.GetLocalSize();
	if (LocalSize.X <= KINDA_SMALL_NUMBER || LocalSize.Y <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FVector2D PixelPosition = FVector2D::ZeroVector;
	FVector2D ViewportPosition = FVector2D::ZeroVector;
	USlateBlueprintLibrary::LocalToViewport(
		this,
		Geometry,
		LocalSize * 0.5f,
		PixelPosition,
		ViewportPosition);
	if (!FMath::IsFinite(PixelPosition.X) || !FMath::IsFinite(PixelPosition.Y))
	{
		return false;
	}

	OutPixelPosition = PixelPosition;
	return true;
}

void USDayHUD::RefreshForegroundReadouts(const USChefGameInstance& GameInstance)
{
	// Widget components rebuild their page on stream in/out, so stale handles mean "look again".
	const bool bNeedsForegroundReadouts = !CoinAmountText.IsValid()
		&& !RevenueCurrentText.IsValid()
		&& !RevenueTargetText.IsValid()
		&& !IngredientCountLingGuText.IsValid()
		&& !IngredientCountYinShanJunText.IsValid()
		&& !IngredientCountChiYanJiaoText.IsValid()
		&& !IngredientCountYueLinYuText.IsValid()
		&& !IngredientCountXuanYuQinText.IsValid()
		&& !BusinessTimeRemainingText.IsValid();
	const bool bNeedsBoxIngredientCounts = !IngredientBinCountLingGuText.IsValid()
		&& !IngredientBinCountYinShanJunText.IsValid()
		&& !IngredientBinCountChiYanJiaoText.IsValid()
		&& !IngredientBinCountYueLinYuText.IsValid()
		&& !IngredientBinCountXuanYuQinText.IsValid();
	if (bNeedsForegroundReadouts || bNeedsBoxIngredientCounts)
	{
		ResolveForegroundReadouts();
	}

	// The art paints bare digits, so bypass locale grouping separators.
	if (UTextBlock* Coin = CoinAmountText.Get())
	{
		Coin->SetText(FText::AsNumber(GameInstance.GetCoinBalance(), &FNumberFormattingOptions::DefaultNoGrouping()));
	}
	if (UTextBlock* Current = RevenueCurrentText.Get())
	{
		Current->SetText(FText::AsNumber(GameInstance.Revenue, &FNumberFormattingOptions::DefaultNoGrouping()));
	}
	if (UTextBlock* Target = RevenueTargetText.Get())
	{
		Target->SetText(FText::AsNumber(GameInstance.RevenueTarget, &FNumberFormattingOptions::DefaultNoGrouping()));
	}

	auto RefreshIngredientCount = [&GameInstance](
		const TWeakObjectPtr<UTextRenderComponent>& BoxReadout,
		const TWeakObjectPtr<UTextBlock>& ForegroundFallback,
		const FName IngredientId)
	{
		const FText QuantityText = FText::FromString(FString::Printf(
			TEXT("×%d"),
			GameInstance.GetQuantity(IngredientId)));
		if (UTextRenderComponent* Text = BoxReadout.Get())
		{
			Text->SetText(QuantityText);
			Text->SetVisibility(true);
			if (UTextBlock* Fallback = ForegroundFallback.Get())
			{
				Fallback->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		else if (UTextBlock* Fallback = ForegroundFallback.Get())
		{
			Fallback->SetVisibility(ESlateVisibility::HitTestInvisible);
			Fallback->SetText(QuantityText);
		}
	};
	RefreshIngredientCount(IngredientBinCountLingGuText, IngredientCountLingGuText, DayLingGuId);
	RefreshIngredientCount(IngredientBinCountYinShanJunText, IngredientCountYinShanJunText, DayYinShanJunId);
	RefreshIngredientCount(IngredientBinCountChiYanJiaoText, IngredientCountChiYanJiaoText, DayChiYanJiaoId);
	RefreshIngredientCount(IngredientBinCountYueLinYuText, IngredientCountYueLinYuText, DayYueLinYuId);
	RefreshIngredientCount(IngredientBinCountXuanYuQinText, IngredientCountXuanYuQinText, DayXuanYuQinId);

	if (UTextBlock* BusinessTime = BusinessTimeRemainingText.Get())
	{
		const bool bShopOpen = GameInstance.IsShopOpen();
		BusinessTime->SetVisibility(
			bShopOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShopOpen)
		{
			const int32 TotalSeconds = FMath::Max(
				0,
				FMath::CeilToInt32(GameInstance.GetDayTimeRemaining()));
			BusinessTime->SetText(FText::FromString(FString::Printf(
				TEXT("%02d:%02d"),
				TotalSeconds / 60,
				TotalSeconds % 60)));
		}
	}
}

void USDayHUD::PlayRevenueFlyFromWorld(const FVector& SourceWorldLocation, const int32 RevenueAmount)
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController || RevenueAmount <= 0)
	{
		return;
	}

	if (!RevenueFlyingItemClass)
	{
		RevenueFlyingItemClass = LoadClass<UUserWidget>(
			nullptr,
			TEXT("/Game/Day/UI/WBP_FlyingItem.WBP_FlyingItem_C"));
	}
	if (!RevenueFlyPathCurve)
	{
		RevenueFlyPathCurve = LoadObject<UCurveFloat>(
			nullptr,
			TEXT("/Game/Day/UI/C_RevenueFlyPath.C_RevenueFlyPath"));
	}
	if (!RevenueFlyScaleCurve)
	{
		RevenueFlyScaleCurve = LoadObject<UCurveFloat>(
			nullptr,
			TEXT("/Game/Day/UI/C_RevenueFlyScale.C_RevenueFlyScale"));
	}
	if (!RevenueFlyingItemMaterial)
	{
		RevenueFlyingItemMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Game/Day/Art/food/M_RevenueCoin.M_RevenueCoin"));
	}
	if (!RevenueFlyingItemClass || !RevenueFlyPathCurve || !RevenueFlyScaleCurve || !RevenueFlyingItemMaterial)
	{
		UE_LOG(LogTemp, Error, TEXT("[DayRevenueFly] Missing flying-item class, curve, or UI material."));
		return;
	}

	FVector2D StartPosition;
	// AddToViewport expects coordinates in the complete game viewport. Passing true here removes the
	// constrained camera-view origin, which shifts the coin whenever aspect-ratio bars are present.
	if (!PlayerController->ProjectWorldLocationToScreen(SourceWorldLocation, StartPosition, false))
	{
		return;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0)
	{
		return;
	}

	// Resolve the actual camera image rectangle inside the viewport. This honors aspect-ratio bars
	// without changing the camera, and keeps "top-left" tied to the rendered image rather than PIE chrome.
	FIntRect CameraViewRect(FIntPoint::ZeroValue, FIntPoint(ViewportWidth, ViewportHeight));
	if (const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		if (LocalPlayer->ViewportClient && LocalPlayer->ViewportClient->Viewport)
		{
			FSceneViewProjectionData ProjectionData;
			if (LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData)
				&& ProjectionData.GetConstrainedViewRect().Width() > 0
				&& ProjectionData.GetConstrainedViewRect().Height() > 0)
			{
				CameraViewRect = ProjectionData.GetConstrainedViewRect();
			}
		}
	}

	StartPosition += RevenueFlySourceScreenOffset;
	FVector2D EndPosition = FVector2D::ZeroVector;
	const bool bResolvedWidgetTarget = ResolveRevenueFlyTargetPosition(EndPosition);
	if (!bResolvedWidgetTarget)
	{
		const FVector2D ClampedTargetRatio(
			FMath::Clamp(RevenueFlyTargetViewportRatio.X, 0.0f, 1.0f),
			FMath::Clamp(RevenueFlyTargetViewportRatio.Y, 0.0f, 1.0f));
		EndPosition = FVector2D(
			CameraViewRect.Min.X + CameraViewRect.Width() * ClampedTargetRatio.X,
			CameraViewRect.Min.Y + CameraViewRect.Height() * ClampedTargetRatio.Y);
		EndPosition += RevenueFlyTargetScreenOffset;
		if (!bRevenueFlyTargetFallbackWarningLogged)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[DayRevenueFly] No valid UMG target geometry was available; using the legacy viewport-ratio fallback."));
			bRevenueFlyTargetFallbackWarningLogged = true;
		}
	}
	if (bClampRevenueFlyToViewport)
	{
		const float CameraMinX = static_cast<float>(CameraViewRect.Min.X);
		const float CameraMinY = static_cast<float>(CameraViewRect.Min.Y);
		const float CameraMaxX = static_cast<float>(CameraViewRect.Max.X);
		const float CameraMaxY = static_cast<float>(CameraViewRect.Max.Y);
		StartPosition.X = FMath::Clamp(StartPosition.X, CameraMinX, CameraMaxX);
		StartPosition.Y = FMath::Clamp(StartPosition.Y, CameraMinY, CameraMaxY);
		const float TargetMaxX = bResolvedWidgetTarget ? static_cast<float>(ViewportWidth) : CameraMaxX;
		const float TargetMaxY = bResolvedWidgetTarget ? static_cast<float>(ViewportHeight) : CameraMaxY;
		const float TargetMinX = bResolvedWidgetTarget ? 0.0f : CameraMinX;
		const float TargetMinY = bResolvedWidgetTarget ? 0.0f : CameraMinY;
		EndPosition.X = FMath::Clamp(EndPosition.X, TargetMinX, TargetMaxX);
		EndPosition.Y = FMath::Clamp(EndPosition.Y, TargetMinY, TargetMaxY);
	}

	auto SetVectorProperty = [](UObject* Object, const FName Name, const FVector& Value)
	{
		if (FStructProperty* Property = FindFProperty<FStructProperty>(Object->GetClass(), Name))
		{
			void* Storage = Property->ContainerPtrToValuePtr<void>(Object);
			Property->CopyCompleteValue(Storage, &Value);
		}
	};
	auto SetNumberProperty = [](UObject* Object, const FName Name, const double Value)
	{
		if (FNumericProperty* Property = FindFProperty<FNumericProperty>(Object->GetClass(), Name))
		{
			void* Storage = Property->ContainerPtrToValuePtr<void>(Object);
			Property->SetFloatingPointPropertyValue(Storage, Value);
		}
	};
	auto SetObjectProperty = [](UObject* Object, const FName Name, UObject* Value)
	{
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name))
		{
			Property->SetObjectPropertyValue_InContainer(Object, Value);
		}
	};
	auto InitializeFlyingItem = [&](UUserWidget* FlyingItem,
		const FVector& ItemStart,
		const FVector& ItemEnd,
		const FVector& ItemControl,
		const double Duration,
		const double StartDelay,
		const double MaxScale)
	{
		// WBP_FlyingItem already owns the author's Init function. Invoke that public contract so the
		// Blueprint's assignments run exactly as authored instead of relying on member-name reflection.
		if (UFunction* InitFunction = FlyingItem->FindFunction(TEXT("Init")))
		{
			FStructOnScope Parameters(InitFunction);
			uint8* ParameterMemory = Parameters.GetStructMemory();
			auto SetVectorParameter = [&](const FName Name, const FVector& Value)
			{
				if (FStructProperty* Property = FindFProperty<FStructProperty>(InitFunction, Name))
				{
					void* Storage = Property->ContainerPtrToValuePtr<void>(ParameterMemory);
					Property->CopyCompleteValue(Storage, &Value);
				}
			};
			auto SetNumberParameter = [&](const FName Name, const double Value)
			{
				if (FNumericProperty* Property = FindFProperty<FNumericProperty>(InitFunction, Name))
				{
					void* Storage = Property->ContainerPtrToValuePtr<void>(ParameterMemory);
					Property->SetFloatingPointPropertyValue(Storage, Value);
				}
			};
			auto SetObjectParameter = [&](const FName Name, UObject* Value)
			{
				if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(InitFunction, Name))
				{
					Property->SetObjectPropertyValue_InContainer(ParameterMemory, Value);
				}
			};

			SetVectorParameter(TEXT("InStartPos"), ItemStart);
			SetVectorParameter(TEXT("InEndPos"), ItemEnd);
			SetVectorParameter(TEXT("InCtrlPos"), ItemControl);
			SetNumberParameter(TEXT("InDuration"), Duration);
			SetNumberParameter(TEXT("InStartDelay"), StartDelay);
			SetNumberParameter(TEXT("InMaxScale"), MaxScale);
			SetObjectParameter(TEXT("InPathCurve"), RevenueFlyPathCurve);
			SetObjectParameter(TEXT("InScaleCurve"), RevenueFlyScaleCurve);
			FlyingItem->ProcessEvent(InitFunction, ParameterMemory);
			return;
		}

		// Compatibility fallback for an older/minimal flying-item Widget without Init.
		SetVectorProperty(FlyingItem, TEXT("StartPos"), ItemStart);
		SetVectorProperty(FlyingItem, TEXT("EndPos"), ItemEnd);
		SetVectorProperty(FlyingItem, TEXT("CtrlPos"), ItemControl);
		SetNumberProperty(FlyingItem, TEXT("Duration"), Duration);
		SetNumberProperty(FlyingItem, TEXT("StartDelay"), StartDelay);
		SetNumberProperty(FlyingItem, TEXT("MaxScale"), MaxScale);
		SetObjectProperty(FlyingItem, TEXT("PathCurve"), RevenueFlyPathCurve);
		SetObjectProperty(FlyingItem, TEXT("ScaleCurve"), RevenueFlyScaleCurve);
	};

	const int32 MinItemCount = FMath::Max(1, FMath::Min(RevenueFlyMinItemCount, RevenueFlyMaxItemCount));
	const int32 MaxItemCount = FMath::Max(MinItemCount, FMath::Max(RevenueFlyMinItemCount, RevenueFlyMaxItemCount));
	const float MinDuration = FMath::Max(0.01f, FMath::Min(RevenueFlyMinDuration, RevenueFlyMaxDuration));
	const float MaxDuration = FMath::Max(MinDuration, FMath::Max(RevenueFlyMinDuration, RevenueFlyMaxDuration));
	const float MinScale = FMath::Max(0.0f, FMath::Min(RevenueFlyMinScale, RevenueFlyMaxScale));
	const float MaxScale = FMath::Max(MinScale, FMath::Max(RevenueFlyMinScale, RevenueFlyMaxScale));
	const int32 ItemCount = FMath::Clamp(MinItemCount + RevenueAmount / 20, MinItemCount, MaxItemCount);
	const FVector2D ControlPosition(
		(StartPosition.X + EndPosition.X) * 0.5f,
		FMath::Min(StartPosition.Y, EndPosition.Y) - CameraViewRect.Height() * FMath::Clamp(RevenueFlyArcHeightRatio, 0.0f, 1.0f));
	for (int32 ItemIndex = 0; ItemIndex < ItemCount; ++ItemIndex)
	{
		UUserWidget* FlyingItem = CreateWidget<UUserWidget>(PlayerController, RevenueFlyingItemClass);
		if (!FlyingItem)
		{
			continue;
		}

		const FVector2D Jitter(FMath::FRandRange(-22.0f, 22.0f), FMath::FRandRange(-14.0f, 14.0f));
		const FVector2D ItemStart = StartPosition + Jitter;
		InitializeFlyingItem(
			FlyingItem,
			FVector(ItemStart, 0.0f),
			FVector(EndPosition, 0.0f),
			FVector(ControlPosition + Jitter * 0.35f, 0.0f),
			FMath::FRandRange(MinDuration, MaxDuration),
			ItemIndex * FMath::Max(0.0f, RevenueFlyItemInterval),
			FMath::FRandRange(MinScale, MaxScale));

		// Projection and placement now both use complete game-viewport coordinates.
		FlyingItem->AddToViewport(150);
		FlyingItem->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
		FlyingItem->SetPositionInViewport(ItemStart, true);
		if (UImage* Icon = Cast<UImage>(FlyingItem->GetWidgetFromName(TEXT("Img_Icon"))))
		{
			if (RevenueFlyingItemMaterial)
			{
				Icon->SetBrushFromMaterial(RevenueFlyingItemMaterial);
			}
			Icon->SetDesiredSizeOverride(FVector2D(54.0f, 54.0f));
		}
	}
}
#pragma endregion K2 moonyfli

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
void USDayHUD::HandleChromeVisibilityChanged(const bool bIsChecked)
{
	ApplyChromeVisibility(bIsChecked);
}

void USDayHUD::ApplyChromeVisibility(const bool bShow)
{
	if (ControlsHost)
	{
		(void)bShow;
		ControlsHost->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!bShow && CheatPanel)
	{
		CheatPanel->SetPanelVisible(false);
	}
}

void USDayHUD::HandleToggleCheatPanel()
{
	// Disabled in all configurations; retained for Blueprint/API compatibility.
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
