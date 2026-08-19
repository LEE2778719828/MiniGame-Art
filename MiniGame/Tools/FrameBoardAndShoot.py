"""Point the editor viewport straight at the SDay.Board face and take a screenshot."""

import unreal


LEVEL_PATH = "/Game/Day/Test/L_S_DayWhitebox"
BOARD_TAG = "SDay.Board"
# The plane fit says the pan faces pitch -34.9 / yaw 90, so view it head-on.
VIEW_PITCH = -34.9
VIEW_YAW = 90.0
DISTANCE = 1600.0


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    target = None
    for actor in subsystem.get_all_level_actors():
        if BOARD_TAG in [str(tag) for tag in actor.get_editor_property("tags")]:
            target = actor
            break
    if target is None:
        unreal.log_error("[BoardShot] no board actor")
        return

    origin, _ = target.get_actor_bounds(False)
    rotation = unreal.Rotator(roll=0.0, pitch=VIEW_PITCH, yaw=VIEW_YAW)
    forward = rotation.get_forward_vector()
    location = unreal.Vector(
        origin.x - forward.x * DISTANCE,
        origin.y - forward.y * DISTANCE,
        origin.z - forward.z * DISTANCE)

    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    editor.set_level_viewport_camera_info(location, rotation)
    unreal.log("[BoardShot] viewport at ({:.1f}, {:.1f}, {:.1f}) looking pitch={} yaw={}".format(
        location.x, location.y, location.z, VIEW_PITCH, VIEW_YAW))

    unreal.AutomationLibrary.take_high_res_screenshot(
        1600, 1600, "E:/UEProjects/MiniGame/MiniGame/Saved/BoardShot.png")


main()
