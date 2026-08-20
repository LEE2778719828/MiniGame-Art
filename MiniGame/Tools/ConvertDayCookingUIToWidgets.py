"""Replace the camera Blueprint's CookingUI mesh planes with UMG WidgetComponents."""

import unreal


BP_FOLDER = "/Game/Day/Blueprints"
CAMERA_BP_PATH = BP_FOLDER + "/BP_SDayCompositionCamera"
BACKGROUND_WBP_PATH = "/Game/Day/UI/WBP_SDayCookingBackground"
FOREGROUND_WBP_PATH = "/Game/Day/UI/WBP_SDayCookingForeground"
CAMERA_LABEL = "Day_CompositionCamera"
CAMERA_TAG = "SDayCamera"
CAMERA_FOLDER = "Cameras"

BG_DEPTH = 4751.6
FG_DEPTH = 1341.7
BG_FRAME_WIDTH = 1641.5
FG_FRAME_WIDTH = 463.5
ASPECT = 0.45
# The engine clamps a WidgetComponent's render target to 3840x2160
# (WidgetComponent.MaximumRenderTargetWidth/Height), and the quad is built from the render
# target, not from DrawSize. Anything taller silently squashes the art, so the canvas is the
# tallest 9:20 page that survives the clamp.
DRAW_HEIGHT = 2160.0
DRAW_WIDTH = DRAW_HEIGHT * ASPECT
BLEND_MODE = unreal.WidgetBlendMode.TRANSPARENT
FALLBACK_ACTOR_TRANSFORM = unreal.Transform(
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


def log(message):
    unreal.log("[CookingUIWidgets] " + message)


def fail(message):
    unreal.log_error("[CookingUIWidgets] " + message)
    raise RuntimeError(message)


def load(path):
    asset = unreal.load_asset(path)
    if asset is None:
        fail("missing asset " + path)
    return asset


def actor_tags(actor):
    return [str(tag) for tag in actor.get_editor_property("tags")]


def make_blueprint():
    if unreal.EditorAssetLibrary.does_asset_exist(CAMERA_BP_PATH):
        unreal.EditorAssetLibrary.delete_asset(CAMERA_BP_PATH)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "BP_SDayCompositionCamera", BP_FOLDER, unreal.Blueprint, factory)
    if blueprint is None:
        fail("could not recreate " + CAMERA_BP_PATH)
    return blueprint


def root_handle(subsystem, blueprint):
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if isinstance(obj, unreal.SceneComponent):
            return handle
    fail("no root in " + CAMERA_BP_PATH)


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


def capture_camera(actor_subsystem):
    for actor in actor_subsystem.get_all_level_actors():
        component = actor.get_component_by_class(unreal.CameraComponent)
        if component is None:
            continue
        if CAMERA_TAG not in actor_tags(actor) and CAMERA_TAG not in [
            str(tag) for tag in component.get_editor_property("component_tags")
        ]:
            continue
        settings = {}
        for name in (
            "projection_mode",
            "field_of_view",
            "ortho_width",
            "aspect_ratio",
            "constrain_aspect_ratio",
        ):
            settings[name] = component.get_editor_property(name)
        return actor.get_actor_transform(), component.get_world_transform(), settings, actor
    log("no camera instance remains; using the last authored composition framing")
    return FALLBACK_ACTOR_TRANSFORM, FALLBACK_ACTOR_TRANSFORM, dict(FALLBACK_CAMERA_SETTINGS), None


def configure_widget(widget, widget_class, depth, frame_width, tag):
    widget.set_editor_property("widget_class", widget_class)
    widget.set_editor_property("space", unreal.WidgetSpace.WORLD)
    widget.set_editor_property("draw_size", unreal.IntPoint(int(DRAW_WIDTH), int(DRAW_HEIGHT)))
    widget.set_editor_property("pivot", unreal.Vector2D(0.5, 0.5))
    widget.set_editor_property("draw_at_desired_size", False)
    widget.set_editor_property("blend_mode", BLEND_MODE)
    widget.set_editor_property("receive_hardware_input", False)
    # One-sided quads disappear the moment the baked facing and the engine's winding disagree,
    # and these pages are only ever seen from the camera they hang off, so cull nothing.
    widget.set_editor_property("is_two_sided", True)
    widget.set_editor_property("relative_location", unreal.Vector(depth, 0.0, 0.0))
    # The widget quad lives in the component's local YZ plane with its visible face along local
    # +X and backface culling on, so the face has to look back down the camera's view axis:
    # a half turn in camera space, which also keeps the page upright and un-mirrored.
    widget.set_editor_property(
        "relative_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0))
    widget.set_editor_property(
        "relative_scale3d",
        unreal.Vector(
            frame_width / DRAW_WIDTH,
            (frame_width / ASPECT) / DRAW_HEIGHT,
            1.0,
        ),
    )
    widget.set_editor_property("component_tags", [unreal.Name(tag)])
    body = widget.get_editor_property("body_instance")
    body.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
    widget.set_editor_property("body_instance", body)
    widget.set_editor_property("generate_overlap_events", False)


def set_actor_tag(blueprint):
    for _ in range(2):
        cdo = unreal.get_default_object(blueprint.generated_class())
        cdo.set_editor_property("tags", [unreal.Name(CAMERA_TAG)])
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)


def main():
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        fail("stop PIE first")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor_transform, camera_transform, settings, old_actor = capture_camera(actor_subsystem)
    background = load(BACKGROUND_WBP_PATH)
    foreground = load(FOREGROUND_WBP_PATH)

    if old_actor:
        actor_subsystem.destroy_actor(old_actor)
    blueprint = make_blueprint()
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    parent = root_handle(subsystem, blueprint)

    camera_handle, camera = add_component(
        subsystem, blueprint, parent, unreal.CameraComponent, "StageCamera")
    for name, value in settings.items():
        camera.set_editor_property(name, value)
    camera.set_editor_property("component_tags", [unreal.Name(CAMERA_TAG)])

    _, background_widget = add_component(
        subsystem, blueprint, camera_handle, unreal.WidgetComponent, "CookingUI_Background")
    configure_widget(
        background_widget, background.generated_class(), BG_DEPTH, BG_FRAME_WIDTH,
        "SDay.Backdrop",
    )

    _, foreground_widget = add_component(
        subsystem, blueprint, camera_handle, unreal.WidgetComponent, "CookingUI_Foreground")
    configure_widget(
        foreground_widget, foreground.generated_class(), FG_DEPTH, FG_FRAME_WIDTH,
        "SDay.Foreground",
    )

    set_actor_tag(blueprint)
    unreal.EditorAssetLibrary.save_asset(CAMERA_BP_PATH)

    actor = actor_subsystem.spawn_actor_from_object(
        blueprint,
        actor_transform.translation,
        actor_transform.rotation.rotator(),
    )
    if actor is None:
        fail("could not spawn " + CAMERA_BP_PATH)
    actor.set_actor_label(CAMERA_LABEL)
    actor.set_folder_path(unreal.Name(CAMERA_FOLDER))
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    log("done: CookingUI is now two UMG WidgetComponents")


main()
