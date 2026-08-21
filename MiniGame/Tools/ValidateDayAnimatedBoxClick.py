"""Click the first runtime ingredient bin and verify its skeletal animation starts."""

import unreal


def fail(message):
    unreal.log_error("[DayAnimatedBoxClick] " + message)
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

controller = unreal.GameplayStatics.get_player_controller(world, 0)
ingredient_ids = [
    "LingGu",
    "YinShanJun",
    "ChiYanJiao",
    "YueLinYu",
    "XuanYuQin",
]
for index, ingredient_id in enumerate(ingredient_ids):
    bin_actor = next(
        (actor for actor in actors
         if actor.get_class().get_name() == "BP_SDayIngredientBinVisual_C"
         and str(actor.get_editor_property("ingredient_id")) == ingredient_id),
        None)
    if bin_actor is None:
        fail("Missing bin " + ingredient_id)

    screen_position = unreal.GameplayStatics.project_world_to_screen(
        controller, bin_actor.get_actor_location(), False)
    presenter.simulate_pointer_event(screen_position, True)
    presenter.simulate_pointer_event(screen_position, False)

    component_name = f"BoxAnim_{index}"
    animated_box = next(
        (component for component in environment.get_components_by_class(
            unreal.SkeletalMeshComponent)
         if component.get_name() == component_name),
        None)
    if animated_box is None:
        fail("Missing " + component_name)

    unreal.log(
        "[DayAnimatedBoxClick] {} screen={} playing={} position={:.4f}".format(
            ingredient_id,
            screen_position,
            animated_box.is_playing(),
            animated_box.get_position()))
    if not animated_box.is_playing():
        fail(component_name + " did not start")
