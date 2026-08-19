"""Re-fit the Day composition camera to the shipping output resolution.

OrthoWidth is a horizontal measurement, so switching the output from 9:16 to a taller
1440x3200 panel does not change how wide the stall reads, but it does change the aspect the
camera must be constrained to, and it changes where the vertical centre of frame lands.
This keeps the hand-tuned camera rotation and re-solves only zoom and centring.

The large ground plane is excluded from the fit so it cannot shrink the stall in frame.
"""

import math

import unreal


LEVEL_PATH = "/Game/Day/Test/L_S_DayWhitebox"
CAMERA_TAG = "SDayCamera"
ENV_PREFIX = "ENV_Canguan_"
EXCLUDED_LABELS = {"ENV_Canguan_pPlane1"}

OUTPUT_WIDTH = 1440.0
OUTPUT_HEIGHT = 3200.0
# Breathing room around the stall so it never touches the screen edge.
MARGIN = 1.08


def log(message):
    unreal.log("[FitCamera] " + message)


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()

    camera = None
    for actor in actors:
        if CAMERA_TAG in [str(tag) for tag in actor.get_editor_property("tags")]:
            camera = actor
            break
    if camera is None:
        unreal.log_error("[FitCamera] no actor tagged " + CAMERA_TAG)
        return

    # The fit below is orthographic maths (OrthoWidth, distance-independent zoom). Converting a
    # perspective camera here would throw away a deliberate manual switch, so refuse instead.
    component = camera.get_component_by_class(unreal.CameraComponent)
    if component.get_editor_property("projection_mode") != unreal.CameraProjectionMode.ORTHOGRAPHIC:
        unreal.log_error("[FitCamera] camera is perspective; switch it with "
                         "Tools/SetDayCameraProjection.py first if that is what you want")
        return

    rotation = camera.get_actor_rotation()
    pitch = math.radians(rotation.pitch)
    yaw = math.radians(rotation.yaw)
    forward = (math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw), math.sin(pitch))
    right = (-math.sin(yaw), math.cos(yaw), 0.0)
    up = (-math.sin(pitch) * math.cos(yaw), -math.sin(pitch) * math.sin(yaw), math.cos(pitch))

    corners = []
    for actor in actors:
        label = actor.get_actor_label()
        if not label.startswith(ENV_PREFIX) or label in EXCLUDED_LABELS:
            continue
        origin, extent = actor.get_actor_bounds(False)
        for sx in (-1.0, 1.0):
            for sy in (-1.0, 1.0):
                for sz in (-1.0, 1.0):
                    corners.append((origin.x + extent.x * sx,
                                    origin.y + extent.y * sy,
                                    origin.z + extent.z * sz))
    if not corners:
        unreal.log_error("[FitCamera] no subject actors found")
        return

    def project(axis):
        values = [sum(corner[i] * axis[i] for i in range(3)) for corner in corners]
        return min(values), max(values)

    right_lo, right_hi = project(right)
    up_lo, up_hi = project(up)
    forward_lo, forward_hi = project(forward)

    aspect = OUTPUT_WIDTH / OUTPUT_HEIGHT
    needed_width = (right_hi - right_lo) * MARGIN
    needed_height = (up_hi - up_lo) * MARGIN
    ortho_width = max(needed_width, needed_height * aspect)
    log("subject spans {:.0f} wide x {:.0f} tall on screen; aspect {:.4f} -> ortho {:.0f}".format(
        right_hi - right_lo, up_hi - up_lo, aspect, ortho_width))
    log("visible area at that zoom: {:.0f} x {:.0f}".format(ortho_width, ortho_width / aspect))

    # Centre the subject in frame while keeping the camera's distance along its view axis.
    location = camera.get_actor_location()
    current = (location.x, location.y, location.z)
    forward_at = sum(current[i] * forward[i] for i in range(3))
    target_right = (right_lo + right_hi) * 0.5
    target_up = (up_lo + up_hi) * 0.5
    # Stand off far enough that nothing crosses the near plane.
    target_forward = min(forward_at, forward_lo - 1000.0)
    new_location = unreal.Vector(
        right[0] * target_right + up[0] * target_up + forward[0] * target_forward,
        right[1] * target_right + up[1] * target_up + forward[1] * target_forward,
        right[2] * target_right + up[2] * target_up + forward[2] * target_forward)

    camera.set_actor_location(new_location, False, False)
    component.set_editor_property("ortho_width", ortho_width)
    component.set_editor_property("constrain_aspect_ratio", True)
    component.set_editor_property("aspect_ratio", aspect)

    saved = unreal.EditorLoadingAndSavingUtils.save_current_level()
    log("camera at ({:.1f}, {:.1f}, {:.1f}) pitch={:.1f} yaw={:.1f} ortho={:.0f} "
        "aspect={:.4f} saved={}".format(
            new_location.x, new_location.y, new_location.z,
            rotation.pitch, rotation.yaw, ortho_width, aspect, saved))


main()
