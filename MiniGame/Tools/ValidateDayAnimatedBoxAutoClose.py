"""Click one ingredient bin in PIE and sample the lid animation until it rests closed."""

import time

import unreal

SAMPLE_SECONDS = 3.0
INGREDIENT_ID = "LingGu"
BIN_INDEX = 0


def fail(message):
    unreal.log_error("[DayBoxAutoClose] " + message)
    raise RuntimeError(message)


world = unreal.get_editor_subsystem(
    unreal.UnrealEditorSubsystem).get_game_world()
if world is None:
    fail("PIE is not running")

actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
presenter = next(
    (actor for actor in actors if actor.get_class().get_name() == "BP_SDayBoardPresenter_C"),
    None)
environment = next(
    (actor for actor in actors if "SDay.Environment" in [
        str(tag) for tag in actor.get_editor_property("tags")]),
    None)
if presenter is None or environment is None:
    fail("Missing presenter or environment")

bin_actor = next(
    (actor for actor in actors
     if actor.get_class().get_name() == "BP_SDayIngredientBinVisual_C"
     and str(actor.get_editor_property("ingredient_id")) == INGREDIENT_ID),
    None)
if bin_actor is None:
    fail("Missing bin " + INGREDIENT_ID)

component_name = "BoxAnim_{}".format(BIN_INDEX)
animated_box = next(
    (component for component in environment.get_components_by_class(
        unreal.SkeletalMeshComponent)
     if component.get_name() == component_name),
    None)
if animated_box is None:
    fail("Missing " + component_name)

animation = unreal.load_asset(
    "/Game/Day/Art/canguan/animation/box{}_Anim".format(BIN_INDEX + 1))
unreal.log("[DayBoxAutoClose] component PlayRate={} sequence RateScale={} length={:.4f}".format(
    animated_box.get_editor_property("animation_data").saved_play_rate,
    animation.get_editor_property("rate_scale"),
    animation.get_play_length()))
unreal.log("[DayBoxAutoClose] before click playing={} position={:.4f}".format(
    animated_box.is_playing(), animated_box.get_position()))

screen_position = unreal.GameplayStatics.project_world_to_screen(
    controller := unreal.GameplayStatics.get_player_controller(world, 0),
    bin_actor.get_actor_location(), False)
presenter.simulate_pointer_event(screen_position, True)
presenter.simulate_pointer_event(screen_position, False)

state = {"start": time.time(), "peak": 0.0, "peak_at": 0.0, "rest_at": None, "handle": None}


def sample(_delta_seconds):
    elapsed = time.time() - state["start"]
    position = animated_box.get_position()
    if position > state["peak"]:
        state["peak"] = position
        state["peak_at"] = elapsed
    if state["rest_at"] is None and state["peak"] > 0.0 and not animated_box.is_playing():
        state["rest_at"] = elapsed
    unreal.log("[DayBoxAutoClose] t={:.3f} playing={} position={:.4f}".format(
        elapsed, animated_box.is_playing(), position))
    if elapsed >= SAMPLE_SECONDS:
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.log(
            "[DayBoxAutoClose] RESULT peak={:.4f} at t={:.3f} rest at t={} final={:.4f} playing={}".format(
                state["peak"], state["peak_at"], state["rest_at"], position,
                animated_box.is_playing()))


state["handle"] = unreal.register_slate_post_tick_callback(sample)
unreal.log("[DayBoxAutoClose] sampling started at screen=" + str(screen_position))
