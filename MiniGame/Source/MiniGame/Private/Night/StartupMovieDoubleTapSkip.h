#pragma once

#include "CoreMinimal.h"

/**
 * Startup PV (NightCourseIntro) double-tap skip.
 *
 * Engine pitfalls this avoids:
 * - DefaultGameMoviePlayer::OnAnyDown only skips when IsLoadingFinished() AND
 *   bMoviesAreSkippable; with our ini that is False, so single tap does nothing.
 * - StopMovie() alone is weaker than ForceCompletion() (sets bUserCalledFinish +
 *   ends the streamer). Call ForceCompletion from the Slate/wait path.
 * - SetSlateOverlayWidget no-ops unless an ActiveMovieStreamer exists, so
 *   scene-travel loading screens that only show a widget are not overwritten.
 * - Overlay is installed from MiniGame Default-phase StartupModule, which runs
 *   after PlayEarlyStartupMovies and before WaitForMovieToFinish.
 */
namespace MiniGameStartupMovieSkip
{
	void Install();
	void Uninstall();
}
