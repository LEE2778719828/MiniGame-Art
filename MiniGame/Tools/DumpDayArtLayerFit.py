"""Report the solved cookingUI layer stack for the day level, to compare edit time against PIE.

FitDayArtLayers places every tagged layer in camera space, so the check that the editor preview and
the game view agree is simply whether the layers carry the same relative depth and fill at edit time
as they do in a session. Results also go to a file under Saved/, because unreal.log does not always
reach stdout when the commandlet host shuts down straight after the script.
"""

import unreal


LEVEL_PATH = "/Game/Day/Maps/L_S_DayWhitebox"
CAMERA_TAG = "SDayCamera"
RIG_CLASS_PATH = "/Script/MiniGame.SDayCameraRig"
LAYER_TAGS = ("SDay.Backdrop", "SDay.Foreground")
REPORT_NAME = "DayArtLayerFit.txt"


def component_tags(component):
    return [str(tag) for tag in component.get_editor_property("component_tags")]


def find_camera_actor(actors):
    for actor in actors:
        for component in actor.get_components_by_class(unreal.CameraComponent):
            tagged_actor = CAMERA_TAG in [str(tag) for tag in actor.get_editor_property("tags")]
            if tagged_actor or CAMERA_TAG in component_tags(component):
                return actor, component
    return None, None


def describe_layer(component):
    location = component.get_editor_property("relative_location")
    scale = component.get_editor_property("relative_scale3d")
    return "{:<28} depth={:>10.2f} scale=({:.6f}, {:.6f}) tags={}".format(
        component.get_name(), location.x, scale.x, scale.y,
        ",".join(component_tags(component)))


def main():
    lines = []

    def report(message):
        lines.append(message)
        unreal.log("[DayArtLayerFit] " + message)

    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()

    actor, camera = find_camera_actor(actors)
    if actor is None:
        report("no camera tagged {} in {}".format(CAMERA_TAG, LEVEL_PATH))
    else:
        rig_class = unreal.load_class(None, RIG_CLASS_PATH)
        on_rig = rig_class is not None and unreal.MathLibrary.class_is_child_of(
            actor.get_class(), rig_class)
        report("camera actor {} class={} derives_from_rig={}".format(
            actor.get_actor_label(), actor.get_class().get_name(), on_rig))
        report("lens ortho={} fov={:.4f} aspect={:.4f} constrain={}".format(
            camera.get_editor_property("ortho_width"),
            camera.get_editor_property("field_of_view"),
            camera.get_editor_property("aspect_ratio"),
            camera.get_editor_property("constrain_aspect_ratio")))

        for component in actor.get_components_by_class(unreal.SceneComponent):
            tags = component_tags(component)
            if any(tag in tags for tag in LAYER_TAGS):
                report(describe_layer(component))

    path = unreal.Paths.combine([unreal.Paths.project_saved_dir(), REPORT_NAME])
    with open(unreal.Paths.convert_relative_path_to_full(path), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")


main()
