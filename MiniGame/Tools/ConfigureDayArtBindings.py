"""Apply semantic gameplay tags to the placed Day restaurant art."""

import unreal


LEVEL_PATH = "/Game/Day/Test/L_S_DayWhitebox"
LABEL_TO_TAG = {
    "ENV_Canguan_pan": "SDay.Board",
    "ENV_Canguan_box6": "SDay.Bin.0",
    "ENV_Canguan_box7": "SDay.Bin.1",
    "ENV_Canguan_box8": "SDay.Bin.2",
    "ENV_Canguan_box9": "SDay.Bin.3",
    "ENV_Canguan_polySurface6": "SDay.Bin.4",
    "ENV_Canguan_tai1": "SDay.Counter",
    # Dish plates along the top of frame; the presenter seats the customer portraits behind them.
    "ENV_Canguan_kepan": "SDay.CustomerPlates",
}


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = {
        actor.get_actor_label(): actor
        for actor in subsystem.get_all_level_actors()
    }

    # Dressing is visual-only. Gameplay traces must reach the hidden cell/bin/seat
    # proxies instead of stopping on the restaurant shell or furniture.
    for label, actor in actors.items():
        if not label.startswith("ENV_Canguan_"):
            continue
        actor.set_actor_enable_collision(False)
        tags = list(actor.get_editor_property("tags"))
        if "SDay.Environment" not in [str(tag) for tag in tags]:
            tags.append(unreal.Name("SDay.Environment"))
            actor.set_editor_property("tags", tags)
        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component:
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            component.set_editor_property("generate_overlap_events", False)

    missing = []
    for label, tag in LABEL_TO_TAG.items():
        actor = actors.get(label)
        if actor is None:
            missing.append(label)
            continue

        tags = [
            existing
            for existing in actor.get_editor_property("tags")
            if not (
                str(existing) == "SDay.Board"
                or str(existing).startswith("SDay.Bin.")
                or str(existing) == "SDay.Counter"
                or str(existing) == "SDay.CustomerPlates"
            )
        ]
        tags.append(unreal.Name(tag))
        actor.set_editor_property("tags", tags)

        unreal.log("[DayArtBinding] {} -> {}".format(label, tag))

    if missing:
        unreal.log_error("[DayArtBinding] missing actors: {}".format(", ".join(missing)))
        return

    saved = unreal.EditorLoadingAndSavingUtils.save_current_level()
    unreal.log("[DayArtBinding] configured {} actors, saved={}".format(
        len(LABEL_TO_TAG), saved))


main()
