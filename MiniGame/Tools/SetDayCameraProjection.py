# Switch the tagged composition camera between perspective and orthographic, and nothing else.
#
# Framing is hand-tuned in the editor, so the projection mode has to be changeable without a
# script re-solving zoom or position behind your back: FitDayCameraToResolution.py used to
# force orthographic as a side effect, which silently undid a manual switch to perspective.
#
#   py Tools/SetDayCameraProjection.py perspective
#   py Tools/SetDayCameraProjection.py orthographic
#   py Tools/SetDayCameraProjection.py            (report only)

import sys

import unreal


CAMERA_TAG = "SDayCamera"

MODES = {
    "perspective": unreal.CameraProjectionMode.PERSPECTIVE,
    "orthographic": unreal.CameraProjectionMode.ORTHOGRAPHIC,
}


def describe(actor, component):
    return "{} mode={} fov={:.1f} ortho={:.1f} aspect={:.4f} constrain={}".format(
        actor.get_actor_label(),
        component.get_editor_property("projection_mode"),
        component.get_editor_property("field_of_view"),
        component.get_editor_property("ortho_width"),
        component.get_editor_property("aspect_ratio"),
        component.get_editor_property("constrain_aspect_ratio"))


def main():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor.get_game_world() is not None:
        unreal.log_error("[DayCamera] stop PIE first; PIE edits do not persist")
        return

    requested = None
    for argument in sys.argv[1:]:
        key = argument.lstrip("-").lower()
        if key in MODES:
            requested = key
    if requested is None and len(sys.argv) > 1:
        unreal.log_error("[DayCamera] expected 'perspective' or 'orthographic'")
        return

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actor_subsystem.get_all_level_actors():
        if CAMERA_TAG not in [str(tag) for tag in actor.get_editor_property("tags")]:
            continue
        component = actor.get_component_by_class(unreal.CameraComponent)
        if component is None:
            continue

        unreal.log("[DayCamera] before: " + describe(actor, component))
        if requested is None:
            return
        component.set_editor_property("projection_mode", MODES[requested])
        unreal.log("[DayCamera] after:  " + describe(actor, component))
        unreal.log("[DayCamera] loc={} rot={} (untouched)".format(
            actor.get_actor_location(), actor.get_actor_rotation()))
        unreal.log("[DayCamera] saved={}".format(
            unreal.EditorLoadingAndSavingUtils.save_current_level()))
        return

    unreal.log_error("[DayCamera] no actor tagged " + CAMERA_TAG)


main()
