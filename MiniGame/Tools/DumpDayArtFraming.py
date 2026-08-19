"""Report the framing inputs needed to retune the Day board camera, lights and cells.

Prints the composition camera, the tagged art bounds and the cell positions the presenter
currently derives from SDay.Board, so the numbers can be compared without guessing.
"""

import unreal


LEVEL_PATH = "/Game/Day/Test/L_S_DayWhitebox"
CAMERA_LABEL = "Day_CompositionCamera"

# Mirrors DayBoardPresentationPrivate in SDayBoardPresentation.cpp.
REFERENCE_BOARD_WIDTH = 830.0
REFERENCE_BOARD_CENTER = (0.0, 150.0, 20.0)
REFERENCE_CELLS = [
    (1, -185, 450, 35), (2, 150, 445, 35), (4, -285, 300, 35), (5, -65, 290, 35),
    (6, 175, 300, 35), (7, 330, 230, 35), (8, -305, 105, 35), (9, -75, 105, 35),
    (10, 165, 100, 35), (11, 320, -25, 35), (13, -165, -125, 35), (14, 125, -150, 35),
]


def log(message):
    unreal.log("[DayFraming] " + message)


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = subsystem.get_all_level_actors()

    board = None
    for actor in actors:
        if actor.get_actor_label() == CAMERA_LABEL:
            component = actor.get_component_by_class(unreal.CameraComponent)
            location = actor.get_actor_location()
            rotation = actor.get_actor_rotation()
            log("camera loc=({:.1f}, {:.1f}, {:.1f}) pitch={:.1f} yaw={:.1f} "
                "ortho={:.1f} aspect={:.4f} constrain={}".format(
                    location.x, location.y, location.z,
                    rotation.pitch, rotation.yaw,
                    component.get_editor_property("ortho_width"),
                    component.get_editor_property("aspect_ratio"),
                    component.get_editor_property("constrain_aspect_ratio")))

    lo = [float("inf")] * 3
    hi = [float("-inf")] * 3
    for actor in actors:
        label = actor.get_actor_label()
        if not label.startswith("ENV_Canguan_"):
            continue
        origin, extent = actor.get_actor_bounds(False)
        tags = [str(tag) for tag in actor.get_editor_property("tags")]
        log("{:<26} x {:>8.1f}..{:<8.1f} y {:>8.1f}..{:<8.1f} z {:>8.1f}..{:<8.1f} {}".format(
            label,
            origin.x - extent.x, origin.x + extent.x,
            origin.y - extent.y, origin.y + extent.y,
            origin.z - extent.z, origin.z + extent.z,
            ",".join(tags)))
        if "SDay.Board" in tags:
            board = (origin, extent)
        if label != "ENV_Canguan_pPlane1":
            for axis, (o, e) in enumerate(
                ((origin.x, extent.x), (origin.y, extent.y), (origin.z, extent.z))
            ):
                lo[axis] = min(lo[axis], o - e)
                hi[axis] = max(hi[axis], o + e)

    log("subject bounds (no ground) x {:.1f}..{:.1f} y {:.1f}..{:.1f} z {:.1f}..{:.1f}".format(
        lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))

    if board is None:
        unreal.log_error("[DayFraming] no SDay.Board actor found")
        return

    origin, extent = board
    scale = (extent.x * 2.0) / REFERENCE_BOARD_WIDTH
    surface_z = origin.z + extent.z
    log("board centre=({:.1f}, {:.1f}, {:.1f}) extent=({:.1f}, {:.1f}, {:.1f})".format(
        origin.x, origin.y, origin.z, extent.x, extent.y, extent.z))
    log("derived scale={:.4f} surfaceZ={:.1f}".format(scale, surface_z))

    for index, rx, ry, rz in REFERENCE_CELLS:
        # BuildCells mirrors X before aligning, matching MirrorX in the presenter.
        mx = -rx
        log("  cell {:>2} -> ({:>8.1f}, {:>8.1f}, {:>8.1f})".format(
            index,
            origin.x + (mx - REFERENCE_BOARD_CENTER[0]) * scale,
            origin.y + (ry - REFERENCE_BOARD_CENTER[1]) * scale,
            surface_z + (rz - REFERENCE_BOARD_CENTER[2]) * scale))


main()
