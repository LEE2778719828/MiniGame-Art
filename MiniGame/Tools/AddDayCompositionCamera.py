"""Add (or adopt) the Day whitebox composition camera.

ASDayBoardPresenter creates its CameraComponent only during BeginPlay, so there is no
camera actor to select, pilot or adjust while dressing the level. This CameraActor
provides that workflow, and because it carries the SDayCamera tag the presenter also
adopts it as the PIE view target, so editor framing and runtime framing stay identical.

An existing camera is adopted transform-and-all: only the tag and the editor-only flag
are enforced, so hand-tuned framing survives a re-run. Framing is solved from the placed
canguan actors only when spawning a fresh camera; the large ground plane is excluded so
it does not make the stall tiny in frame.
"""

import math

import unreal


LEVEL_PATH = "/Game/Day/Test/L_S_DayWhitebox"
CAMERA_LABEL = "Day_CompositionCamera"
LEGACY_CAMERA_LABEL = "EDITORONLY_Day_CompositionCamera"
CAMERA_FOLDER = "Environment/Cameras"
# Must match ASDayBoardPresenter::LevelCameraTag.
CAMERA_TAG = "SDayCamera"
ENV_PREFIX = "ENV_Canguan_"
EXCLUDED_LABELS = {"ENV_Canguan_pPlane1"}

PITCH = -60.0
YAW = 90.0
DISTANCE = 2600.0
# Shipping output is a 1440x3200 portrait panel; see Tools/FitDayCameraToResolution.py.
ASPECT_RATIO = 1440.0 / 3200.0
WIDTH_MARGIN = 1.12
MIN_ORTHO_WIDTH = 1200.0


def log(message):
    unreal.log("[CompositionCamera] " + message)


def union_actor_bounds(actors):
    lo = [float("inf")] * 3
    hi = [float("-inf")] * 3

    for actor in actors:
        origin, extent = actor.get_actor_bounds(False)
        for axis, (o, e) in enumerate(
            ((origin.x, extent.x), (origin.y, extent.y), (origin.z, extent.z))
        ):
            lo[axis] = min(lo[axis], o - e)
            hi[axis] = max(hi[axis], o + e)

    return lo, hi


def solve_framing(actor_subsystem):
    subjects = [
        actor
        for actor in actor_subsystem.get_all_level_actors()
        if actor.get_actor_label().startswith(ENV_PREFIX)
        and actor.get_actor_label() not in EXCLUDED_LABELS
    ]
    if not subjects:
        return None

    lo, hi = union_actor_bounds(subjects)
    centre = (
        (lo[0] + hi[0]) * 0.5,
        (lo[1] + hi[1]) * 0.5,
        (lo[2] + hi[2]) * 0.5,
    )
    ortho_width = max(MIN_ORTHO_WIDTH, (hi[0] - lo[0]) * WIDTH_MARGIN)

    pitch_rad = math.radians(PITCH)
    yaw_rad = math.radians(YAW)
    forward = (
        math.cos(pitch_rad) * math.cos(yaw_rad),
        math.cos(pitch_rad) * math.sin(yaw_rad),
        math.sin(pitch_rad),
    )
    location = unreal.Vector(
        centre[0] - forward[0] * DISTANCE,
        centre[1] - forward[1] * DISTANCE,
        centre[2] - forward[2] * DISTANCE,
    )
    return location, unreal.Rotator(roll=0.0, pitch=PITCH, yaw=YAW), ortho_width


def find_existing(actor_subsystem):
    for actor in actor_subsystem.get_all_level_actors():
        if actor.get_actor_label() in (CAMERA_LABEL, LEGACY_CAMERA_LABEL):
            return actor
    return None


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    camera = find_existing(actor_subsystem)
    adopted = camera is not None

    if camera is None:
        framing = solve_framing(actor_subsystem)
        if framing is None:
            unreal.log_error("[CompositionCamera] no canguan actors found")
            return
        location, rotation, ortho_width = framing
        camera = actor_subsystem.spawn_actor_from_class(
            unreal.CameraActor, location, rotation
        )
        if camera is None:
            unreal.log_error("[CompositionCamera] failed to spawn CameraActor")
            return
        camera.set_folder_path(CAMERA_FOLDER)

        component = camera.get_component_by_class(unreal.CameraComponent)
        component.set_editor_property(
            "projection_mode", unreal.CameraProjectionMode.ORTHOGRAPHIC
        )
        component.set_editor_property("ortho_width", ortho_width)
        component.set_editor_property("constrain_aspect_ratio", True)
        component.set_editor_property("aspect_ratio", ASPECT_RATIO)

    camera.set_actor_label(CAMERA_LABEL)
    # The presenter needs this actor at runtime, so it must not be stripped from PIE.
    camera.set_editor_property("is_editor_only_actor", False)

    tags = list(camera.get_editor_property("tags"))
    if CAMERA_TAG not in [str(tag) for tag in tags]:
        tags.append(unreal.Name(CAMERA_TAG))
        camera.set_editor_property("tags", tags)

    actor_subsystem.set_selected_level_actors([camera])
    saved = unreal.EditorLoadingAndSavingUtils.save_current_level()

    location = camera.get_actor_location()
    rotation = camera.get_actor_rotation()
    component = camera.get_component_by_class(unreal.CameraComponent)
    log(
        "{} '{}' location=({:.0f}, {:.0f}, {:.0f}) rotation=(pitch {:.0f}, yaw {:.0f}) "
        "ortho={:.0f} tags={} editor_only={} saved={}".format(
            "adopted" if adopted else "spawned",
            CAMERA_LABEL,
            location.x,
            location.y,
            location.z,
            rotation.pitch,
            rotation.yaw,
            component.get_editor_property("ortho_width"),
            [str(tag) for tag in camera.get_editor_property("tags")],
            camera.get_editor_property("is_editor_only_actor"),
            saved,
        )
    )


main()
