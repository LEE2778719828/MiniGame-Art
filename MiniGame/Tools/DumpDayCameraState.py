# Report the tagged composition camera: package on disk, transform and projection settings.

import unreal


CAMERA_TAG = "SDayCamera"


def main():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_editor_world() or editor.get_game_world()
    if world is None:
        unreal.log_error("[DayCamera] no world")
        return

    # BP_SDayCanguan packs the camera as its StageCamera component, so the tag can be on either the
    # actor or the component, and only the component transform is the framing.
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        component = actor.get_component_by_class(unreal.CameraComponent)
        if component is None:
            continue
        tagged_actor = CAMERA_TAG in [str(tag) for tag in actor.get_editor_property("tags")]
        if not tagged_actor and not component.component_has_tag(CAMERA_TAG):
            continue
        unreal.log("[DayCamera] label={} component={} package={}".format(
            actor.get_actor_label(), component.get_name(),
            actor.get_package().get_path_name()))
        unreal.log("[DayCamera] location={} rotation={}".format(
            component.get_world_location(), component.get_world_rotation()))
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
