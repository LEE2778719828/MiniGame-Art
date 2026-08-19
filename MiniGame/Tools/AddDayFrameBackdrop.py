# Give the Day composition a backdrop that fills the camera frame exactly.
#
# The restaurant art floats in a black void, and the letterbox bars of the portrait frame are
# black too, so there is no way to tell where the rendered picture ends. A plane sized to the
# camera frame makes that edge readable: backdrop colour is inside the frame, black is outside
# it. Works for either projection - a perspective frame is solved at the plane's own depth.
#
# The plane is unlit so lighting and exposure tweaks cannot change what the frame edge looks
# like. ASDayBoardPresenter re-fits it from the live camera at BeginPlay, so PIE stays honest
# even if the camera moves; rerun this script to refresh the editor viewport as well.

import math

import unreal


LEVEL_PATH = "/Game/Day/Test/L_S_DayWhitebox"
CAMERA_TAG = "SDayCamera"
BACKDROP_TAG = "SDay.Backdrop"
ACTOR_LABEL = "ENV_FrameBackdrop"
ACTOR_FOLDER = "Environment/FrameBackdrop"

PLANE_PATH = "/Engine/BasicShapes/Plane.Plane"
# The engine plane is 100 x 100 cm at scale 1.
PLANE_SIZE = 100.0

MATERIAL_DIR = "/Game/Day/Art"
MATERIAL_NAME = "M_SDayFrameBackdrop"
# Dark blue-grey: clearly not the black outside the frame, and clearly not the grey props.
BACKDROP_COLOR = (0.10, 0.13, 0.18)

# Clearance kept behind the furthest piece of restaurant art.
DEPTH_MARGIN = 200.0


def log(message):
    unreal.log("[FrameBackdrop] " + message)


def backdrop_material():
    package_path = "{}/{}".format(MATERIAL_DIR, MATERIAL_NAME)
    existing = unreal.load_asset(package_path)
    if existing is not None:
        return existing

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MATERIAL_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        unreal.log_error("[FrameBackdrop] could not create " + package_path)
        return None

    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    colour = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -300, 0)
    colour.set_editor_property("constant", unreal.LinearColor(*BACKDROP_COLOR))
    unreal.MaterialEditingLibrary.connect_material_property(
        colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(package_path)
    log("created " + package_path)
    return material


def camera_frame(actor_subsystem):
    for actor in actor_subsystem.get_all_level_actors():
        if CAMERA_TAG in [str(tag) for tag in actor.get_editor_property("tags")]:
            component = actor.camera_component
            transform = component.get_world_transform()
            rotation = transform.rotation.rotator()
            return (
                transform.translation,
                unreal.MathLibrary.get_forward_vector(rotation),
                unreal.MathLibrary.get_right_vector(rotation),
                component,
            )
    return None


def frame_width_at_depth(component, depth):
    """Orthographic zoom is distance-independent; perspective widens with distance.

    The project constrains the horizontal FOV, so FieldOfView is the horizontal angle.
    """
    if component.get_editor_property("projection_mode") == unreal.CameraProjectionMode.ORTHOGRAPHIC:
        return component.get_editor_property("ortho_width")
    half_angle = math.radians(0.5 * component.get_editor_property("field_of_view"))
    return 2.0 * depth * math.tan(half_angle)


def art_depth(actor_subsystem, origin, forward):
    """How far the furthest piece of dressing sits along the view axis."""
    depth = 0.0
    for actor in actor_subsystem.get_all_level_actors():
        if "SDay.Environment" not in [str(tag) for tag in actor.get_editor_property("tags")]:
            continue
        centre, extent = actor.get_actor_bounds(False)
        radius = math.sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z)
        along_view = ((centre.x - origin.x) * forward.x
                      + (centre.y - origin.y) * forward.y
                      + (centre.z - origin.z) * forward.z)
        depth = max(depth, along_view + radius)
    return depth


def main():
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor_subsystem.get_game_world() is not None:
        unreal.log_error("[FrameBackdrop] stop PIE first")
        return

    current = editor_subsystem.get_editor_world()
    if current is None or current.get_path_name().split(".")[0] != LEVEL_PATH:
        unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    frame = camera_frame(actor_subsystem)
    if frame is None:
        unreal.log_error("[FrameBackdrop] no camera tagged " + CAMERA_TAG)
        return
    origin, forward, right, component = frame
    aspect_ratio = component.get_editor_property("aspect_ratio")
    if aspect_ratio <= 0.0:
        unreal.log_error("[FrameBackdrop] camera has no constrained aspect ratio")
        return

    material = backdrop_material()
    plane = unreal.load_asset(PLANE_PATH)
    if plane is None:
        unreal.log_error("[FrameBackdrop] missing " + PLANE_PATH)
        return

    for actor in actor_subsystem.get_all_level_actors():
        if actor.get_actor_label() == ACTOR_LABEL:
            actor_subsystem.destroy_actor(actor)

    depth = art_depth(actor_subsystem, origin, forward) + DEPTH_MARGIN
    location = unreal.Vector(
        origin.x + forward.x * depth,
        origin.y + forward.y * depth,
        origin.z + forward.z * depth)
    # Face the camera: plane normal is local +Z, and its surface spans local X / Y.
    rotation = unreal.MathLibrary.make_rot_from_zx(
        unreal.Vector(-forward.x, -forward.y, -forward.z), right)

    actor = actor_subsystem.spawn_actor_from_object(plane, location, rotation)
    if actor is None:
        unreal.log_error("[FrameBackdrop] failed to spawn the backdrop")
        return

    frame_width = frame_width_at_depth(component, depth)
    if frame_width <= 0.0:
        unreal.log_error("[FrameBackdrop] camera frame has no width")
        return
    frame_height = frame_width / aspect_ratio
    actor.set_actor_label(ACTOR_LABEL)
    actor.set_folder_path(ACTOR_FOLDER)
    actor.set_actor_scale3d(unreal.Vector(
        frame_width / PLANE_SIZE, frame_height / PLANE_SIZE, 1.0))
    actor.set_actor_enable_collision(False)
    actor.set_editor_property("tags", [
        unreal.Name("SDay.Environment"), unreal.Name(BACKDROP_TAG)])

    mesh_component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if mesh_component:
        mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        mesh_component.set_editor_property("generate_overlap_events", False)
        if material:
            mesh_component.set_material(0, material)

    log("{} frame {:.0f} x {:.0f} at depth {:.0f}".format(
        component.get_editor_property("projection_mode"), frame_width, frame_height, depth))
    log("saved={}".format(unreal.EditorLoadingAndSavingUtils.save_current_level()))


main()
