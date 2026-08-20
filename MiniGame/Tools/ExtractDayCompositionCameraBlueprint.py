"""Extract StageCamera and its CookingUI children into a standalone Blueprint actor.

The level keeps two independent actors after this script:

    ENV_Canguan              restaurant meshes only
    Day_CompositionCamera    camera + CookingUI layers

The authored world transforms are captured before either old Blueprint is replaced.
Run with the Day whitebox level open and PIE stopped.
"""

import unreal


BP_FOLDER = "/Game/Day/Blueprints"
CANGUAN_BP_PATH = BP_FOLDER + "/BP_SDayCanguan"
CAMERA_BP_NAME = "BP_SDayCompositionCamera"
CAMERA_BP_PATH = BP_FOLDER + "/" + CAMERA_BP_NAME
MESH_FOLDER = "/Game/Day/Art/canguan"

ACTOR_LABEL = "ENV_Canguan"
CAMERA_LABEL = "Day_CompositionCamera"
ACTOR_FOLDER = "Environment/Canguan"
CAMERA_FOLDER = "Cameras"
ACTOR_TAG = "SDay.Environment"
CAMERA_TAG = "SDayCamera"

FALLBACK_CAMERA_TRANSFORM = unreal.Transform(
    unreal.Vector(30.278877, -2760.044691, 22.658015),
    unreal.Rotator(roll=0.0, pitch=-11.0, yaw=90.0),
    unreal.Vector(1.0, 1.0, 1.0),
)
FALLBACK_CAMERA_SETTINGS = {
    "projection_mode": unreal.CameraProjectionMode.PERSPECTIVE,
    "field_of_view": 19.59999656677246,
    "ortho_width": 984.4000244140625,
    "aspect_ratio": 0.44999998807907104,
    "constrain_aspect_ratio": True,
}
FALLBACK_CANGUAN_TRANSFORM = unreal.Transform(
    unreal.Vector(44.813791, -600.891635, -1276.702209),
    unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0),
    unreal.Vector(1.0, 1.0, 1.0),
)
FALLBACK_STALL_SCALE = unreal.Vector(3.891920, 3.891920, 3.891920)

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


def log(message):
    unreal.log("[ExtractCamera] " + message)


def fail(message):
    unreal.log_error("[ExtractCamera] " + message)
    raise RuntimeError(message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        fail("missing asset " + path)
    return asset


def tags(component):
    return [str(tag) for tag in component.get_editor_property("component_tags")]


def actor_tags(actor):
    return [str(tag) for tag in actor.get_editor_property("tags")]


def make_blueprint(name):
    path = BP_FOLDER + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, BP_FOLDER, unreal.Blueprint, factory)
    if blueprint is None:
        fail("could not create " + path)
    return blueprint


def root_handle(subsystem, blueprint):
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if isinstance(obj, unreal.SceneComponent):
            return handle
    fail("no scene root in " + blueprint.get_path_name())


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


def set_actor_tag(blueprint, tag):
    for _ in range(2):
        cdo = unreal.get_default_object(blueprint.generated_class())
        if cdo is None:
            fail("no CDO for " + blueprint.get_path_name())
        cdo.set_editor_property("tags", [unreal.Name(tag)])
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)


def copy_camera_property(source, target, name):
    target.set_editor_property(name, source.get_editor_property(name))


def capture_level(actor_subsystem):
    canguan = None
    old_camera_actor = None
    camera_component = None
    camera_transform = None
    camera_settings = {}
    layer_specs = []
    stall_scale = unreal.Vector(1.0, 1.0, 1.0)

    for actor in actor_subsystem.get_all_level_actors():
        label = actor.get_actor_label()
        tags_on_actor = actor_tags(actor)
        component = actor.get_component_by_class(unreal.CameraComponent)
        if component and (
            CAMERA_TAG in tags_on_actor or CAMERA_TAG in tags(component)
        ):
            old_camera_actor = actor
            camera_component = component
            camera_transform = component.get_world_transform()
            for name in (
                "projection_mode",
                "field_of_view",
                "ortho_width",
                "aspect_ratio",
                "constrain_aspect_ratio",
            ):
                camera_settings[name] = component.get_editor_property(name)
            for child in component.get_children_components(False):
                child_tags = tags(child)
                if not child_tags:
                    continue
                if not isinstance(child, unreal.StaticMeshComponent):
                    continue
                layer_specs.append({
                    "name": child.get_name(),
                    "mesh": child.static_mesh,
                    "material": child.get_material(0),
                    "relative_transform": child.get_relative_transform(),
                    "tags": child_tags,
                })
        if label == ACTOR_LABEL:
            canguan = actor
            stall = actor.get_component_by_class(unreal.SceneComponent)
            for candidate in actor.get_components_by_class(unreal.SceneComponent):
                if candidate.get_name() == "Stall":
                    stall = candidate
                    break
            if stall:
                stall_scale = stall.get_editor_property("relative_scale3d")

    if canguan is None:
        log("no restaurant instance remains; using the last authored canguan transform")
        canguan_transform = FALLBACK_CANGUAN_TRANSFORM
        stall_scale = FALLBACK_STALL_SCALE
    else:
        canguan_transform = canguan.get_actor_transform()
    if camera_component is None:
        # The previous failed attempt already removed the packed instance. Preserve the last
        # authored framing so the extraction remains recoverable and deterministic.
        log("no camera instance remains; using the last authored composition framing")
        camera_transform = FALLBACK_CAMERA_TRANSFORM
        camera_settings = dict(FALLBACK_CAMERA_SETTINGS)
        face_camera = unreal.MathLibrary.make_rot_from_zx(
            unreal.Vector(-1.0, 0.0, 0.0), unreal.Vector(0.0, 1.0, 0.0))
        fallback_layers = [
            ("CookingBG_00", "/Game/Day/Art/cookingUI/MI_CookingBG_01_Street", 4751.6, 16.415, 36.477, ["SDay.Backdrop", "SDay.BackdropOrder.0"]),
            ("CookingBG_01", "/Game/Day/Art/cookingUI/MI_CookingBG_02_Crowd", 4736.6, 16.363, 36.362, ["SDay.Backdrop", "SDay.BackdropOrder.1"]),
            ("CookingBG_02", "/Game/Day/Art/cookingUI/MI_CookingBG_03_Storefront", 4721.6, 16.311, 36.247, ["SDay.Backdrop", "SDay.BackdropOrder.2"]),
            ("CookingBG_03", "/Game/Day/Art/cookingUI/MI_CookingBG_04_Interior", 4706.6, 16.259, 36.132, ["SDay.Backdrop", "SDay.BackdropOrder.3"]),
            ("CookingFG_00", "/Game/Day/Art/cookingUI/MI_CookingFG_01_Coins", 1341.7, 4.635, 10.300, ["SDay.Foreground", "SDay.ForegroundOrder.0"]),
            ("CookingFG_01", "/Game/Day/Art/cookingUI/MI_CookingFG_02_Tally", 1326.7, 4.583, 10.185, ["SDay.Foreground", "SDay.ForegroundOrder.1"]),
            ("CookingFG_02", "/Game/Day/Art/cookingUI/MI_CookingFG_03_Rope", 1311.7, 4.531, 10.070, ["SDay.Foreground", "SDay.ForegroundOrder.2"]),
        ]
        for name, material_path, depth, sx, sy, layer_tags in fallback_layers:
            layer_specs.append({
                "name": name,
                "mesh": load("/Engine/BasicShapes/Plane.Plane"),
                "material": load(material_path),
                "relative_transform": unreal.Transform(
                    unreal.Vector(depth, 0.0, 0.0), face_camera,
                    unreal.Vector(sx, sy, 1.0)),
                "tags": layer_tags,
            })
    if len(layer_specs) != 7:
        fail("expected 7 CookingUI layers, found {}".format(len(layer_specs)))
    return (
        canguan_transform,
        stall_scale,
        camera_transform,
        camera_settings,
        layer_specs,
        canguan,
        old_camera_actor,
    )


def destroy_old_actors(actor_subsystem, canguan, old_camera_actor):
    if old_camera_actor and old_camera_actor != canguan:
        actor_subsystem.destroy_actor(old_camera_actor)
    if canguan:
        actor_subsystem.destroy_actor(canguan)


def build_camera_blueprint(camera_transform, settings, layer_specs):
    blueprint = make_blueprint(CAMERA_BP_NAME)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    parent = root_handle(subsystem, blueprint)

    camera_handle, camera = add_component(
        subsystem, blueprint, parent, unreal.CameraComponent, "StageCamera")
    for name, value in settings.items():
        camera.set_editor_property(name, value)
    camera.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))
    camera.set_editor_property(
        "relative_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    camera.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
    camera.set_editor_property("component_tags", [unreal.Name(CAMERA_TAG)])

    for spec in layer_specs:
        _, layer = add_component(
            subsystem, blueprint, camera_handle, unreal.StaticMeshComponent, spec["name"])
        layer.set_editor_property("static_mesh", spec["mesh"])
        transform = spec["relative_transform"]
        layer.set_editor_property("relative_location", transform.translation)
        layer.set_editor_property("relative_rotation", transform.rotation.rotator())
        layer.set_editor_property("relative_scale3d", transform.scale3d)
        layer.set_editor_property("component_tags", [unreal.Name(t) for t in spec["tags"]])
        layer.set_editor_property("generate_overlap_events", False)
        body = layer.get_editor_property("body_instance")
        body.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
        layer.set_editor_property("body_instance", body)
        if spec["material"]:
            layer.set_material(0, spec["material"])

    set_actor_tag(blueprint, CAMERA_TAG)
    unreal.EditorAssetLibrary.save_asset(CAMERA_BP_PATH)
    return blueprint


def build_canguan_blueprint(stall_scale):
    blueprint = make_blueprint("BP_SDayCanguan")
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    parent = root_handle(subsystem, blueprint)
    stall_handle, stall = add_component(
        subsystem, blueprint, parent, unreal.SceneComponent, "Stall")
    stall.set_editor_property("relative_scale3d", stall_scale)

    for name, piece_tags in PIECES:
        _, component = add_component(
            subsystem, blueprint, stall_handle, unreal.StaticMeshComponent, name)
        component.set_editor_property(
            "static_mesh", load("{}/{}.{}".format(MESH_FOLDER, name, name)))
        component.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 0.0))
        component.set_editor_property(
            "relative_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
        component.set_editor_property("relative_scale3d", unreal.Vector(1.0, 1.0, 1.0))
        if piece_tags:
            component.set_editor_property(
                "component_tags", [unreal.Name(t) for t in piece_tags])

    set_actor_tag(blueprint, ACTOR_TAG)
    unreal.EditorAssetLibrary.save_asset(CANGUAN_BP_PATH)
    return blueprint


def main():
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        fail("stop PIE first")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    (
        canguan_transform,
        stall_scale,
        camera_transform,
        camera_settings,
        layer_specs,
        canguan,
        old_camera_actor,
    ) = capture_level(actor_subsystem)

    destroy_old_actors(actor_subsystem, canguan, old_camera_actor)
    # Both old assets can now be replaced without leaving placed instances referencing them.
    canguan_blueprint = build_canguan_blueprint(stall_scale)
    camera_blueprint = build_camera_blueprint(
        camera_transform, camera_settings, layer_specs)

    restaurant = actor_subsystem.spawn_actor_from_object(
        canguan_blueprint,
        canguan_transform.translation,
        canguan_transform.rotation.rotator(),
    )
    if restaurant is None:
        fail("could not spawn " + CANGUAN_BP_PATH)
    restaurant.set_actor_label(ACTOR_LABEL)
    restaurant.set_folder_path(unreal.Name(ACTOR_FOLDER))

    camera_actor = actor_subsystem.spawn_actor_from_object(
        camera_blueprint,
        camera_transform.translation,
        camera_transform.rotation.rotator(),
    )
    if camera_actor is None:
        fail("could not spawn " + CAMERA_BP_PATH)
    camera_actor.set_actor_label(CAMERA_LABEL)
    camera_actor.set_folder_path(unreal.Name(CAMERA_FOLDER))

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    log("done: restaurant and camera are independent level actors")
    log("camera blueprint: " + CAMERA_BP_PATH)
    log("restaurant blueprint: " + CANGUAN_BP_PATH)


main()
