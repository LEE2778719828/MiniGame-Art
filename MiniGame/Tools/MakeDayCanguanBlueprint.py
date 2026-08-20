"""Legacy all-in-one stage builder.

The current workflow uses ExtractDayCompositionCameraBlueprint.py, which keeps the restaurant and
composition camera as independent Blueprint actors. This file remains as the older rebuild
reference and should not be used for the current level.

Layout:

    BP_SDayCanguan (Actor)              actor transform: canguan pivot, Yaw 180, scale 1
    |- DefaultSceneRoot
       |- Stall (SceneComponent)        scale 3.89192, the only scaled node
       |  |- 12 StaticMeshComponent     relative identity; SDay.* gameplay tags
       |- StageCamera (CameraComponent) relative transform reproduces the authored world framing
          |- CookingBG_00..03           full-frame planes behind the stall
          |- CookingFG_00..02           full-frame planes in front of it

canguan.fbx exported every piece with its pivot on a shared origin, so the twelve meshes keep the
authored layout with relative identity transforms. The scale lives on Stall instead of the actor so
the camera and its layers stay at scale 1, which is what makes the layer fit arithmetic honest.

The cookingUI planes are children of the camera, so moving or re-rotating the camera carries the
whole picture with it. Only depth and size are solved, and the same arithmetic runs again in
ASDayBoardPresenter::BeginPlay (FitDayArtLayers), so the editor viewport and PIE agree.

Use the current extraction from the editor with the Day whitebox level open and PIE stopped:
    py "<project>/MiniGame/Tools/ExtractDayCompositionCameraBlueprint.py"
"""

import math

import unreal

BP_FOLDER = "/Game/Day/Blueprints"
BP_NAME = "BP_SDayCanguan"
BP_PATH = "{}/{}".format(BP_FOLDER, BP_NAME)
MESH_FOLDER = "/Game/Day/Art/canguan"
TEXTURE_FOLDER = "/Game/Day/Art/cookingUI"
MIC_FOLDER = "/Game/Day/Art/cookingUI"
BASE_MATERIAL = "/Game/Day/Art/M_SDayCookingLayer"
PLANE_PATH = "/Engine/BasicShapes/Plane.Plane"
PLANE_SIZE = 100.0

ACTOR_LABEL = "ENV_Canguan"
ACTOR_FOLDER = "Environment/Canguan"
ACTOR_TAG = "SDay.Environment"
CAMERA_TAG = "SDayCamera"
BACKDROP_TAG = "SDay.Backdrop"
FOREGROUND_TAG = "SDay.Foreground"
BACKDROP_ORDER_PREFIX = "SDay.BackdropOrder."
FOREGROUND_ORDER_PREFIX = "SDay.ForegroundOrder."

# Kept in sync with the constants in SDayBoardPresentation.cpp.
FAR_MARGIN = 200.0
NEAR_MARGIN = 120.0
NEAR_MIN_DEPTH = 60.0
LAYER_STEP = 15.0

# Actors this Blueprint replaces, and the assets it supersedes.
LEGACY_LABEL_PREFIXES = ("ENV_Canguan", "ENV_CookingBG_", "ENV_CookingFG_")
LEGACY_ASSETS = ("/Game/Day/Blueprints/BP_SDayCompositionCamera",)

# The shared pivot canguan.fbx exported every piece on, as placed in L_S_DayWhitebox. The stall is
# fixed by the art, so it lives here rather than being read back out of the level.
# unreal.Rotator takes (roll, pitch, yaw) positionally, so spell the angles out.
STALL_LOCATION = unreal.Vector(44.813791, -600.891635, -1276.702209)
STALL_ROTATION = unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0)
STALL_SCALE = unreal.Vector(3.891920, 3.891920, 3.891920)

# Framing fallback, used only when neither a tagged camera actor nor a previous Blueprint instance
# is in the level to read it off. Dial framing in the viewport and rerun to re-bake.
FALLBACK_CAMERA = unreal.Transform(
    unreal.Vector(30.278877, -2760.044691, 22.658015),
    unreal.Rotator(roll=0.0, pitch=-11.0, yaw=90.0),
    unreal.Vector(1.0, 1.0, 1.0))
FALLBACK_CAMERA_SETTINGS = {
    "projection_mode": unreal.CameraProjectionMode.PERSPECTIVE,
    "field_of_view": 19.6,
    "ortho_width": 984.4,
    "aspect_ratio": 0.45,
    "constrain_aspect_ratio": True,
}

# (component name, gameplay tags) in the level's original ordering.
PIECES = [
    ("Mesh_0", []),
    ("box6", ["SDay.Bin.0"]),
    ("pCube6", []),
    ("pPlane1", []),
    ("tai1", ["SDay.Counter"]),
    ("box7", ["SDay.Bin.1"]),
    ("polySurface6", ["SDay.Bin.4"]),
    ("box8", ["SDay.Bin.2"]),
    ("box9", ["SDay.Bin.3"]),
    ("guai", []),
    ("kepan", ["SDay.CustomerPlates"]),
    ("pan", ["SDay.Board"]),
]

# Backdrops go behind the stall; order 0 (street) is furthest, later layers step nearer.
BACKDROP_LAYERS = [
    ("T_CookingUI_Background_01", "MI_CookingBG_01_Street"),
    ("T_CookingUI_Background_02", "MI_CookingBG_02_Crowd"),
    ("T_CookingUI_Background_03", "MI_CookingBG_03_Storefront"),
    ("T_CookingUI_Background_04", "MI_CookingBG_04_Interior"),
]
# Overlays go in front of the stall; order 0 (coins) sits behind the rope (highest order).
FOREGROUND_LAYERS = [
    ("T_CookingUI_Overlay_01", "MI_CookingFG_01_Coins"),
    ("T_CookingUI_Overlay_02", "MI_CookingFG_02_Tally"),
    ("T_CookingUI_Overlay_03", "MI_CookingFG_03_Rope"),
]

CAMERA_PROPERTIES = (
    "projection_mode",
    "field_of_view",
    "ortho_width",
    "aspect_ratio",
    "constrain_aspect_ratio",
)


def log(message):
    unreal.log("[canguan] " + message)


def fail(message):
    unreal.log_error("[canguan] " + message)
    raise RuntimeError(message)


def load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        fail("missing asset " + path)
    return asset


def mesh_world_sphere(mesh, stall_transform):
    """(centre, radius) matching UPrimitiveComponent::Bounds under the stall transform.

    The stall only ever yaws by 180 and scales uniformly, which maps an axis-aligned box to an
    axis-aligned box, so the world half-extents are the local ones scaled.
    """
    box = mesh.get_bounding_box()
    centre_local = unreal.Vector(
        0.5 * (box.min.x + box.max.x),
        0.5 * (box.min.y + box.max.y),
        0.5 * (box.min.z + box.max.z))
    scale = stall_transform.scale3d
    extent = unreal.Vector(
        0.5 * (box.max.x - box.min.x) * abs(scale.x),
        0.5 * (box.max.y - box.min.y) * abs(scale.y),
        0.5 * (box.max.z - box.min.z) * abs(scale.z))
    return (unreal.MathLibrary.transform_location(stall_transform, centre_local),
            math.sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z))


def survey_camera(actor_subsystem):
    """Framing, preferring whatever is live in the level over the baked fallback.

    A standalone tagged camera actor wins (pre-Blueprint levels, or a camera temporarily unpacked
    to frame in the viewport); otherwise the StageCamera of the previous Blueprint instance, which
    is the normal rerun path; otherwise the constants above.
    """
    fallback_component = None
    for actor in actor_subsystem.get_all_level_actors():
        try:
            tags = [str(tag) for tag in actor.get_editor_property("tags")]
        except Exception:
            continue
        component = actor.get_component_by_class(unreal.CameraComponent)
        if component is None:
            continue
        if CAMERA_TAG in tags:
            log("framing read off tagged camera actor " + actor.get_actor_label())
            return component.get_world_transform(), read_camera_settings(component)
        if component.component_has_tag(CAMERA_TAG):
            fallback_component = component
    if fallback_component is not None:
        log("framing read off the previous StageCamera component")
        return fallback_component.get_world_transform(), read_camera_settings(fallback_component)
    log("framing read off the baked fallback")
    return FALLBACK_CAMERA, dict(FALLBACK_CAMERA_SETTINGS)


def read_camera_settings(component):
    return {name: component.get_editor_property(name) for name in CAMERA_PROPERTIES}


def bracket_depths(stall_transform, camera_transform):
    """(far, near) view-axis depths bracketing the 3D dressing, in camera-local units."""
    origin = camera_transform.translation
    forward = unreal.MathLibrary.get_forward_vector(camera_transform.rotation.rotator())
    far = 0.0
    near = float("inf")
    for name, _ in PIECES:
        centre, radius = mesh_world_sphere(
            load("{}/{}.{}".format(MESH_FOLDER, name, name)), stall_transform)
        along = ((centre.x - origin.x) * forward.x
                 + (centre.y - origin.y) * forward.y
                 + (centre.z - origin.z) * forward.z)
        far = max(far, along + radius)
        near = min(near, along - radius)
    if near == float("inf"):
        near = far
    # The restaurant includes wide floor and wall planes that reach most of the way back to the
    # camera, so the nearest dressing can be only a couple of metres out. Reserve room for the whole
    # overlay stack above the near clip, or the deepest layers would all clamp to one depth.
    stack_floor = NEAR_MIN_DEPTH + (len(FOREGROUND_LAYERS) - 1) * LAYER_STEP
    return far + FAR_MARGIN, max(near - NEAR_MARGIN, stack_floor)


def frame_width_at_depth(settings, depth):
    if settings["projection_mode"] == unreal.CameraProjectionMode.ORTHOGRAPHIC:
        return settings["ortho_width"]
    return 2.0 * depth * math.tan(math.radians(0.5 * settings["field_of_view"]))


def make_blueprint():
    if unreal.EditorAssetLibrary.does_asset_exist(BP_PATH):
        unreal.EditorAssetLibrary.delete_asset(BP_PATH)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        BP_NAME, BP_FOLDER, unreal.Blueprint, factory)
    if blueprint is None:
        fail("could not create " + BP_PATH)
    return blueprint


def root_handle(subsystem, blueprint):
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if isinstance(obj, unreal.SceneComponent):
            return handle
    fail(BP_PATH + " has no scene root to attach to")


def add_component(subsystem, blueprint, parent, component_class, name):
    params = unreal.AddNewSubobjectParams()
    params.set_editor_property("parent_handle", parent)
    params.set_editor_property("new_class", component_class)
    params.set_editor_property("blueprint_context", blueprint)
    handle, error = subsystem.add_new_subobject(params)
    if error and not error.is_empty():
        fail("adding {} failed: {}".format(name, error))
    subsystem.rename_subobject(handle, unreal.Text(name))
    data = subsystem.k2_find_subobject_data_from_handle(handle)
    return handle, unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)


def set_relative(component, transform):
    component.set_editor_property("relative_location", transform.translation)
    component.set_editor_property("relative_rotation", transform.rotation.rotator())
    component.set_editor_property("relative_scale3d", transform.scale3d)


def tag(component, tags):
    if tags:
        component.set_editor_property("component_tags", [unreal.Name(t) for t in tags])


def layer_material(mic_name, texture_name):
    """Point the layer's material instance at its slice, creating the instance if needed."""
    mic_path = "{}/{}".format(MIC_FOLDER, mic_name)
    mic = unreal.EditorAssetLibrary.load_asset(mic_path)
    if mic is None:
        mic = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            mic_name, MIC_FOLDER, unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        if mic is None:
            fail("could not create " + mic_path)
        mic.set_editor_property("parent", load(BASE_MATERIAL))
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
        mic, "Tex", load("{}/{}".format(TEXTURE_FOLDER, texture_name)))
    unreal.EditorAssetLibrary.save_asset(mic_path)
    return mic


def build_layers(subsystem, blueprint, camera_handle, settings, layers, role_tag,
                 order_prefix, label_prefix, base_depth):
    aspect = settings["aspect_ratio"]
    plane = load(PLANE_PATH)
    # Camera-local: +X is the view axis, +Y screen right, +Z screen up. The plane's normal is
    # local +Z, so face it back down the view axis.
    face_camera = unreal.MathLibrary.make_rot_from_zx(
        unreal.Vector(-1.0, 0.0, 0.0), unreal.Vector(0.0, 1.0, 0.0))
    for order, (texture_name, mic_name) in enumerate(layers):
        depth = base_depth - order * LAYER_STEP
        width = frame_width_at_depth(settings, depth)
        height = width / aspect
        name = "{}{:02d}".format(label_prefix, order)
        _, component = add_component(
            subsystem, blueprint, camera_handle, unreal.StaticMeshComponent, name)
        component.set_editor_property("static_mesh", plane)
        component.set_editor_property("relative_location", unreal.Vector(depth, 0.0, 0.0))
        component.set_editor_property("relative_rotation", face_camera)
        component.set_editor_property(
            "relative_scale3d", unreal.Vector(width / PLANE_SIZE, height / PLANE_SIZE, 1.0))
        body = component.get_editor_property("body_instance")
        body.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("body_instance", body)
        component.set_editor_property("generate_overlap_events", False)
        component.set_material(0, layer_material(mic_name, texture_name))
        tag(component, [role_tag, "{}{}".format(order_prefix, order)])
        log("{} {} '{}' depth {:.0f} frame {:.0f} x {:.0f}".format(
            role_tag, order, texture_name, depth, width, height))


def clear_legacy(actor_subsystem):
    """Clear the level before the asset is recreated: force-deleting the Blueprint while an
    instance is still placed tears the actor out from under us."""
    doomed = []
    for actor in actor_subsystem.get_all_level_actors():
        try:
            label = actor.get_actor_label()
            tags = [str(t) for t in actor.get_editor_property("tags")]
        except Exception:
            continue
        if label.startswith(LEGACY_LABEL_PREFIXES) or CAMERA_TAG in tags:
            doomed.append((label, actor))
    for _, actor in doomed:
        actor_subsystem.destroy_actor(actor)
    log("removed {} superseded actor(s): {}".format(
        len(doomed), ", ".join(label for label, _ in doomed) or "none"))


def main():
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        fail("stop PIE before running this; level edits would not persist")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    # The actor keeps the canguan pivot and its Yaw, but sheds the scale so the camera subtree
    # can stay at scale 1; Stall carries the scale for the meshes instead.
    actor_transform = unreal.Transform(STALL_LOCATION, STALL_ROTATION, unreal.Vector(1.0, 1.0, 1.0))
    stall = unreal.Transform(STALL_LOCATION, STALL_ROTATION, STALL_SCALE)
    camera, settings = survey_camera(actor_subsystem)
    far_depth, near_depth = bracket_depths(stall, camera)
    log("camera loc={} rot={} settings={}".format(
        camera.translation, camera.rotation.rotator(),
        {k: str(v) for k, v in settings.items()}))
    log("bracket far={:.0f} near={:.0f}".format(far_depth, near_depth))

    clear_legacy(actor_subsystem)
    blueprint = make_blueprint()
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    parent = root_handle(subsystem, blueprint)

    stall_handle, stall_component = add_component(
        subsystem, blueprint, parent, unreal.SceneComponent, "Stall")
    set_relative(stall_component, unreal.Transform(
        unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0), stall.scale3d))
    for name, tags in PIECES:
        _, component = add_component(
            subsystem, blueprint, stall_handle, unreal.StaticMeshComponent, name)
        component.set_editor_property(
            "static_mesh", load("{}/{}.{}".format(MESH_FOLDER, name, name)))
        set_relative(component, unreal.Transform(
            unreal.Vector(0.0, 0.0, 0.0), unreal.Rotator(0.0, 0.0, 0.0),
            unreal.Vector(1.0, 1.0, 1.0)))
        tag(component, tags)
        log("stall piece {} tags={}".format(name, tags))

    camera_handle, camera_component = add_component(
        subsystem, blueprint, parent, unreal.CameraComponent, "StageCamera")
    set_relative(camera_component, unreal.MathLibrary.make_relative_transform(
        camera, actor_transform))
    for name, value in settings.items():
        camera_component.set_editor_property(name, value)
    tag(camera_component, [CAMERA_TAG])

    build_layers(subsystem, blueprint, camera_handle, settings, BACKDROP_LAYERS,
                 BACKDROP_TAG, BACKDROP_ORDER_PREFIX, "CookingBG_", far_depth)
    build_layers(subsystem, blueprint, camera_handle, settings, FOREGROUND_LAYERS,
                 FOREGROUND_TAG, FOREGROUND_ORDER_PREFIX, "CookingFG_", near_depth)

    # The tag has to land on the CDO on both sides of the compile: before, so the compile carries
    # it into the generated class, and after, because the compile rebuilds the CDO.
    for _ in range(2):
        cdo = unreal.get_default_object(blueprint.generated_class())
        if cdo is None:
            fail("no class default object for " + BP_PATH)
        cdo.set_editor_property("tags", [unreal.Name(ACTOR_TAG)])
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_asset(BP_PATH)

    actor = actor_subsystem.spawn_actor_from_object(
        blueprint, actor_transform.translation, actor_transform.rotation.rotator())
    if actor is None:
        fail("could not place " + BP_PATH)
    actor.set_actor_label(ACTOR_LABEL)
    actor.set_folder_path(unreal.Name(ACTOR_FOLDER))
    log("placed {} tags={}".format(
        actor.get_actor_label(), [str(t) for t in actor.get_editor_property("tags")]))

    for path in LEGACY_ASSETS:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.EditorAssetLibrary.delete_asset(path)
            log("deleted superseded " + path)

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    log("done")


main()
