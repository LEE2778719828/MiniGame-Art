#include "Night/StartupMovieDoubleTapSkip.h"

#include "MoviePlayer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SNullWidget.h"

namespace MiniGameStartupMovieSkip
{
	namespace Private
	{
		static constexpr double DoubleTapWindowSeconds = 0.45;
		static constexpr TCHAR IntroMovieToken[] = TEXT("NightCourseIntro");

		static FDelegateHandle PlaybackStartedHandle;
		static FDelegateHandle PlaybackFinishedHandle;
		static FDelegateHandle PlaybackTickHandle;
		static TSharedPtr<SWidget> OverlayInstance;
		static double LastTapSeconds = 0.0;
		static bool bHooksInstalled = false;
		/** True only for the boot intro session that was already playing when MiniGame loaded. */
		static bool bBootIntroSession = false;

		static bool IsStartupIntroMoviePlaying()
		{
			if (!IsMoviePlayerEnabled())
			{
				return false;
			}
			IGameMoviePlayer* MoviePlayer = GetMoviePlayer();
			if (!MoviePlayer || !MoviePlayer->IsInitialized())
			{
				return false;
			}
			if (!MoviePlayer->IsMovieCurrentlyPlaying())
			{
				return false;
			}

			const FString MovieName = MoviePlayer->GetMovieName();
			if (MovieName.Contains(IntroMovieToken))
			{
				return true;
			}

			// IsStartupMoviePlaying() is just IsMoviePlaying — it also lights up for
			// scene-travel MoviePlayer loads. Only trust an empty name during the
			// boot session captured at Install().
			return bBootIntroSession && MovieName.IsEmpty();
		}

		static void RequestSkip()
		{
			if (!IsStartupIntroMoviePlaying())
			{
				return;
			}
			if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
			{
				UE_LOG(
					LogTemp,
					Display,
					TEXT("[StartupMovie] Double-tap skip -> ForceCompletion (movie='%s')."),
					*MoviePlayer->GetMovieName());
				MoviePlayer->ForceCompletion();
			}
		}

		static void RegisterTap()
		{
			if (!IsStartupIntroMoviePlaying())
			{
				return;
			}
			const double Now = FPlatformTime::Seconds();
			if (LastTapSeconds > 0.0 && (Now - LastTapSeconds) <= DoubleTapWindowSeconds)
			{
				LastTapSeconds = 0.0;
				RequestSkip();
				return;
			}
			LastTapSeconds = Now;
		}

		class SStartupMovieDoubleTapOverlay : public SCompoundWidget
		{
		public:
			SLATE_BEGIN_ARGS(SStartupMovieDoubleTapOverlay) {}
			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs)
			{
				SetVisibility(EVisibility::Visible);
				ChildSlot
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBorder")))
					.Padding(0.f)
					[
						SNullWidget::NullWidget
					]
				];
			}

			virtual FReply OnMouseButtonDown(
				const FGeometry& MyGeometry,
				const FPointerEvent& MouseEvent) override
			{
				if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
					|| MouseEvent.IsTouchEvent())
				{
					RegisterTap();
					return FReply::Handled();
				}
				return FReply::Unhandled();
			}

			virtual FReply OnTouchStarted(
				const FGeometry& MyGeometry,
				const FPointerEvent& InTouchEvent) override
			{
				RegisterTap();
				return FReply::Handled();
			}
		};

		static void ClearOverlay()
		{
			LastTapSeconds = 0.0;
			OverlayInstance.Reset();
			bBootIntroSession = false;
		}

		static void TryInstallOverlay()
		{
			if (!IsStartupIntroMoviePlaying())
			{
				return;
			}
			IGameMoviePlayer* MoviePlayer = GetMoviePlayer();
			if (!MoviePlayer)
			{
				return;
			}
			if (!OverlayInstance.IsValid())
			{
				OverlayInstance = SNew(SStartupMovieDoubleTapOverlay);
				UE_LOG(
					LogTemp,
					Display,
					TEXT("[StartupMovie] Double-tap skip overlay created (movie='%s')."),
					*MoviePlayer->GetMovieName());
			}
			// SetSlateOverlayWidget no-ops until ActiveMovieStreamer exists; re-apply on tick.
			MoviePlayer->SetSlateOverlayWidget(OverlayInstance);
		}

		static void HandlePlaybackStarted()
		{
			// Only auto-arm for the named intro. Scene-travel loads must not arm skip.
			if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
			{
				if (MoviePlayer->GetMovieName().Contains(IntroMovieToken))
				{
					bBootIntroSession = true;
				}
			}
			TryInstallOverlay();
		}

		static void HandlePlaybackTick(float /*DeltaTime*/)
		{
			TryInstallOverlay();
		}

		static void HandlePlaybackFinished()
		{
			ClearOverlay();
		}
	}

	void Install()
	{
		using namespace Private;
		if (bHooksInstalled)
		{
			return;
		}
		bHooksInstalled = true;

		if (IsMoviePlayerEnabled())
		{
			if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
			{
				PlaybackStartedHandle = MoviePlayer->OnMoviePlaybackStarted().AddStatic(
					&HandlePlaybackStarted);
				PlaybackFinishedHandle = MoviePlayer->OnMoviePlaybackFinished().AddStatic(
					&HandlePlaybackFinished);
				PlaybackTickHandle = MoviePlayer->OnMoviePlaybackTick().AddStatic(
					&HandlePlaybackTick);

				// Default phase loads after PlayEarlyStartupMovies: capture boot intro.
				if (MoviePlayer->IsMovieCurrentlyPlaying())
				{
					bBootIntroSession = true;
				}
			}
		}

		TryInstallOverlay();
	}

	void Uninstall()
	{
		using namespace Private;
		if (!bHooksInstalled)
		{
			return;
		}
		bHooksInstalled = false;

		if (IsMoviePlayerEnabled())
		{
			if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
			{
				if (PlaybackStartedHandle.IsValid())
				{
					MoviePlayer->OnMoviePlaybackStarted().Remove(PlaybackStartedHandle);
				}
				if (PlaybackFinishedHandle.IsValid())
				{
					MoviePlayer->OnMoviePlaybackFinished().Remove(PlaybackFinishedHandle);
				}
				if (PlaybackTickHandle.IsValid())
				{
					MoviePlayer->OnMoviePlaybackTick().Remove(PlaybackTickHandle);
				}
			}
		}
		PlaybackStartedHandle.Reset();
		PlaybackFinishedHandle.Reset();
		PlaybackTickHandle.Reset();
		ClearOverlay();
	}
}
