# Report the exposure lock and key light values the running game actually uses.

import unreal


CAMERA_TAG = "SDayCamera"


def main():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world()
    if world is None:
        unreal.log_error("[Exposure] no PIE world")
        return

    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor):
        if CAMERA_TAG not in [str(tag) for tag in actor.get_editor_property("tags")]:
            continue
        settings = actor.camera_component.get_editor_property("post_process_settings")
        unreal.log("[Exposure] {} blend_weight={}".format(
            actor.get_actor_label(),
            actor.camera_component.get_editor_property("post_process_blend_weight")))
        unreal.log("[Exposure] {} method_override={} method={} min_override={} min={} max_override={} max={} bias={}".format(
            actor.get_actor_label(),
            settings.get_editor_property("override_auto_exposure_method"),
            settings.get_editor_property("auto_exposure_method"),
            settings.get_editor_property("override_auto_exposure_min_brightness"),
            settings.get_editor_property("auto_exposure_min_brightness"),
            settings.get_editor_property("override_auto_exposure_max_brightness"),
            settings.get_editor_property("auto_exposure_max_brightness"),
            settings.get_editor_property("auto_exposure_bias")))

    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if "SDayBoardPresenter" not in actor.get_class().get_name():
            continue
        for component in actor.get_components_by_class(unreal.DirectionalLightComponent):
            unreal.log("[Exposure] {} intensity={} colour={}".format(
                component.get_name(),
                component.get_editor_property("intensity"),
                component.get_editor_property("light_color")))


main()
