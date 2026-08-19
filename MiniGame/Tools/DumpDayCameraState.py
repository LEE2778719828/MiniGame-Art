# Report the tagged composition camera: package on disk, transform and projection settings.

import unreal


CAMERA_TAG = "SDayCamera"


def main():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world() or editor.get_game_world()
    if world is None:
        unreal.log_error("[DayCamera] no world")
        return

    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CameraActor):
        if CAMERA_TAG not in [str(tag) for tag in actor.get_editor_property("tags")]:
            continue
        component = actor.camera_component
        unreal.log("[DayCamera] label={} package={}".format(
            actor.get_actor_label(), actor.get_package().get_path_name()))
        unreal.log("[DayCamera] location={} rotation={}".format(
            actor.get_actor_location(), actor.get_actor_rotation()))
        for name in ("projection_mode", "ortho_width", "field_of_view",
                     "constrain_aspect_ratio", "aspect_ratio",
                     "aspect_ratio_axis_constraint",
                     "auto_calculate_ortho_planes", "ortho_near_clip_plane",
                     "ortho_far_clip_plane"):
            try:
                unreal.log("[DayCamera]   {} = {}".format(
                    name, component.get_editor_property(name)))
            except Exception as ex:
                unreal.log("[DayCamera]   {} unavailable ({})".format(name, ex))
        return

    unreal.log_error("[DayCamera] no camera tagged " + CAMERA_TAG)


main()
