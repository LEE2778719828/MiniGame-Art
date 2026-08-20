# Composition check: dress the Day whitebox level with the canguan restaurant art so the
# runtime board (spawned at world origin by ASDayBoardPresenter) can be judged against the
# concept art instead of a grey void.
#
# Whitebox reference frame, read from SDayBoardPresentation.cpp:
#   board frame  center (0, 150, -25), extent 900 x 800 x 25
#   cells        X +/-330, Y -150..450, visual radius 85
#   counter      Y 737..782, Z 32..97
#   seats        X -315..315, Y 815, Z 105
#   camera       orthographic from (0, 60, 2200), OrthoWidth 1100, aspect 9:16
#                pitch -90 yaw 90 -> screen up is +Y, screen right is -X
#
# The updated FBX bakes its scene transforms, so every prop's local bounds origin already
# encodes the artist's layout (plate shelf at local Y -290, tray at -193, crates near -26,
# chef at local X +87). One shared actor transform therefore reproduces that layout, and
# only its rotation, scale and offset need solving.
#
# Rotation: the art faces up-screen along local -Y and puts the chef at local +X. Yaw 180
# maps local -Y to world +Y (screen up) and local +X to world -X (screen right), matching
# the concept art on both axes.
#
# Height: the tray sits on top of the stall, while the logical board sits at Z ~0. Aligning
# the tray's top face to the board plane is what makes the dishes land in the cells; the
# stall body and floor then fall below the board, which is correct.

import unreal


LEVEL_PATH = "/Game/Day/Maps/L_S_DayWhitebox"
ART_DIR = "/Game/Day/Art/canguan"
LABEL_PREFIX = "ENV_Canguan_"
ACTOR_FOLDER = "Environment/CanguanPreview"

# Superseded by the updated FBX; kept on disk but no longer placed.
LEGACY_MESHES = {
    "polySurface9",
    "polySurface10",
    "polySurface11",
    "polySurface12",
    "pCylinder44",
}

GAMEPLAY_TAGS = {
    "pan": "SDay.Board",
    "box6": "SDay.Bin.0",
    "box7": "SDay.Bin.1",
    "box8": "SDay.Bin.2",
    "box9": "SDay.Bin.3",
    "polySurface6": "SDay.Bin.4",
    "tai1": "SDay.Counter",
    "kepan": "SDay.CustomerPlates",
}

# The tray whose circular slots stand in for the merge cells.
ANCHOR_MESH = "pan"
# Cell span (660) widened by the cell visual radius on each side.
ANCHOR_TARGET_WIDTH_X = 830.0
# Cell field centre: X 0, Y midway between -150 and 450.
ANCHOR_TARGET_XY = (0.0, 150.0)
# Tray top face lands just above the cell surface.
ANCHOR_TOP_Z = 20.0

YAW = 180.0


def log(message):
    unreal.log("[CanguanPreview] " + message)


def collect_meshes():
    meshes = {}
    for path in sorted(unreal.EditorAssetLibrary.list_assets(ART_DIR, recursive=False)):
        asset = unreal.load_asset(path)
        if not isinstance(asset, unreal.StaticMesh):
            continue
        name = asset.get_name()
        if name in LEGACY_MESHES:
            continue
        meshes[name] = asset
    return meshes


def local_box(mesh):
    bounds = mesh.get_bounds()
    origin = bounds.origin
    extent = bounds.box_extent
    return (
        (origin.x - extent.x, origin.x + extent.x),
        (origin.y - extent.y, origin.y + extent.y),
        (origin.z - extent.z, origin.z + extent.z),
    )


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    meshes = collect_meshes()
    if ANCHOR_MESH not in meshes:
        unreal.log_error("[CanguanPreview] anchor mesh '{}' not found in {}".format(
            ANCHOR_MESH, ART_DIR))
        return

    anchor_x, anchor_y, anchor_z = local_box(meshes[ANCHOR_MESH])
    anchor_width = anchor_x[1] - anchor_x[0]
    if anchor_width <= 0.0:
        unreal.log_error("[CanguanPreview] anchor mesh has no width")
        return

    scale = ANCHOR_TARGET_WIDTH_X / anchor_width

    # Yaw 180 negates local X and Y, so the anchor centre maps to -centre * scale.
    anchor_centre_x = (anchor_x[0] + anchor_x[1]) * 0.5
    anchor_centre_y = (anchor_y[0] + anchor_y[1]) * 0.5
    offset_x = ANCHOR_TARGET_XY[0] + anchor_centre_x * scale
    offset_y = ANCHOR_TARGET_XY[1] + anchor_centre_y * scale
    offset_z = ANCHOR_TOP_Z - anchor_z[1] * scale

    location = unreal.Vector(offset_x, offset_y, offset_z)
    log("anchor '{}' width {:.1f} -> scale {:.3f}".format(
        ANCHOR_MESH, anchor_width, scale))
    log("shared transform location=({:.1f}, {:.1f}, {:.1f}) yaw={:.0f}".format(
        location.x, location.y, location.z, YAW))

    # Make reruns deterministic; also clears the previous placement of the old meshes.
    removed = 0
    for actor in actor_subsystem.get_all_level_actors():
        if actor.get_actor_label().startswith(LABEL_PREFIX):
            actor_subsystem.destroy_actor(actor)
            removed += 1
    log("removed {} previous preview actor(s)".format(removed))

    rotation = unreal.Rotator(roll=0.0, pitch=0.0, yaw=YAW)
    for name in sorted(meshes):
        mesh = meshes[name]
        actor = actor_subsystem.spawn_actor_from_object(mesh, location, rotation)
        if actor is None:
            unreal.log_error("[CanguanPreview] failed to spawn: " + name)
            continue

        actor.set_actor_label(LABEL_PREFIX + name)
        actor.set_folder_path(ACTOR_FOLDER)
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        actor.set_actor_enable_collision(False)
        actor_tags = [unreal.Name("SDay.Environment")]
        if name in GAMEPLAY_TAGS:
            actor_tags.append(unreal.Name(GAMEPLAY_TAGS[name]))
        actor.set_editor_property("tags", actor_tags)

        component = actor.get_component_by_class(unreal.StaticMeshComponent)
        if component:
            # The presenter drives cell selection with pointer traces; dressing must not block them.
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            component.set_editor_property("generate_overlap_events", False)

        bx, by, bz = local_box(mesh)
        log("  {:<14} world X {:>7.0f}..{:<7.0f} Y {:>7.0f}..{:<7.0f} Z {:>7.0f}..{:<7.0f}".format(
            name,
            -bx[1] * scale + offset_x, -bx[0] * scale + offset_x,
            -by[1] * scale + offset_y, -by[0] * scale + offset_y,
            bz[0] * scale + offset_z, bz[1] * scale + offset_z,
        ))

    saved = unreal.EditorLoadingAndSavingUtils.save_current_level()
    log("placed {} mesh(es), saved={}".format(len(meshes), saved))


main()
