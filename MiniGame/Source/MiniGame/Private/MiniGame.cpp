#include "MiniGame.h"
#include "Modules/ModuleManager.h"
#include "Night/StartupMovieDoubleTapSkip.h"

#pragma region K2 moonyfli
IMPLEMENT_PRIMARY_GAME_MODULE(FMiniGameModule, MiniGame, "MiniGame");

void FMiniGameModule::StartupModule()
{
	// Default phase: after PlayEarlyStartupMovies, before WaitForMovieToFinish.
	MiniGameStartupMovieSkip::Install();
}

void FMiniGameModule::ShutdownModule()
{
	MiniGameStartupMovieSkip::Uninstall();
}
#pragma endregion K2 moonyfli
