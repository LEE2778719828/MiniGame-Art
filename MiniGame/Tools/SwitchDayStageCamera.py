"""Hand the day stage over to BP_DayCamera, carrying the framing of the camera it replaces.

The presentation layer finds its camera by the SDayCamera tag and fits the cookingUI layers in
camera space, so a swap is three edits: tag BP_DayCamera's camera, park the backdrop page under
that camera with the SDay.Backdrop tag, and let the level hold exactly one tagged camera.
"""

import unreal


CAMERA_BP_PATH = "/Game/Day/Blueprints/BP_DayCamera"
CAMERA_TAG = "SDayCamera"
BACKDROP_TAG = "SDay.Backdrop"
CAMERA_COMPONENT_NAME = "Camera"
BACKDROP_COMPONENT_NAME = "background"
INSTANCE_LABEL = "Day_Camera"
INSTANCE_FOLDER = "Cameras"
CAMERA_SETTING_NAMES = (
    "projection_mode",
    "field_of_view",
    "ortho_width",
    "aspect_ratio",
    "constrain_aspect_ratio",
)


def log(message):
    unreal.log("[SwitchDayCamera] " + message)


def fail(message):
    unreal.log_error("[SwitchDayCamera] " + message)
    raise RuntimeError(message)


def component_tags(component):
    return [str(tag) for tag in component.get_editor_property("component_tags")]


def find_outgoing_camera(actor_subsystem):
    """The tagged camera currently framing the level, whether tagged on the actor or the component."""
    for actor in actor_subsystem.get_all_level_actors():
        for component in actor.get_components_by_class(unreal.CameraComponent):
            tagged_actor = CAMERA_TAG in [str(tag) for tag in actor.get_editor_property("tags")]
            if tagged_actor or CAMERA_TAG in component_tags(component):
                return actor, component
    return None, None


def subobject_nodes(subsystem, blueprint):
    nodes = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is not None:
            nodes[obj.get_name().replace("_GEN_VARIABLE", "")] = (handle, obj)
    return nodes


def main():
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        fail("stop PIE first")

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    outgoing_actor, outgoing_camera = find_outgoing_camera(actor_subsystem)
    if outgoing_actor is None:
        fail("no tagged camera in the level to inherit the framing from")

    actor_transform = outgoing_actor.get_actor_transform()
    camera_relative = outgoing_camera.get_relative_transform()
    settings = {name: outgoing_camera.get_editor_property(name) for name in CAMERA_SETTING_NAMES}
    log("inheriting framing from {}: actor {} camera relative {} settings {}".format(
        outgoing_actor.get_actor_label(), actor_transform, camera_relative, settings))

    blueprint = unreal.load_asset(CAMERA_BP_PATH)
    if blueprint is None:
        fail("missing " + CAMERA_BP_PATH)

    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    nodes = subobject_nodes(subsystem, blueprint)
    if CAMERA_COMPONENT_NAME not in nodes:
        fail("{} has no component named {}".format(CAMERA_BP_PATH, CAMERA_COMPONENT_NAME))

    camera_handle, camera = nodes[CAMERA_COMPONENT_NAME]
    camera.set_editor_property("relative_location", camera_relative.translation)
    camera.set_editor_property("relative_rotation", camera_relative.rotation.rotator())
    for name, value in settings.items():
        camera.set_editor_property(name, value)
    camera.set_editor_property("component_tags", [unreal.Name(CAMERA_TAG)])

    if BACKDROP_COMPONENT_NAME in nodes:
        backdrop_handle, backdrop = nodes[BACKDROP_COMPONENT_NAME]
        # FitDayArtLayers solves the page in camera space, so the page has to hang off the camera:
        # re-framing then carries the picture along instead of leaving it behind in the world.
        if not subsystem.attach_subobject(camera_handle, backdrop_handle):
            fail("could not park {} under the camera".format(BACKDROP_COMPONENT_NAME))
        backdrop.set_editor_property("component_tags", [unreal.Name(BACKDROP_TAG)])
        # A one-sided quad vanishes whenever the baked facing and the engine's winding disagree,
        # and this page is only ever seen from the camera it hangs off, so cull nothing.
        backdrop.set_editor_property("is_two_sided", True)
    else:
        log("no {} component; the camera will run without a backdrop page".format(
            BACKDROP_COMPONENT_NAME))

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_asset(CAMERA_BP_PATH)

    actor = actor_subsystem.spawn_actor_from_object(
        blueprint, actor_transform.translation, actor_transform.rotation.rotator())
    if actor is None:
        fail("could not place " + CAMERA_BP_PATH)
    actor.set_actor_scale3d(actor_transform.scale3d)
    actor.set_actor_label(INSTANCE_LABEL)
    actor.set_folder_path(unreal.Name(INSTANCE_FOLDER))

    # Two tagged cameras would make the view target a coin toss, so the old one goes.
    log("removing outgoing camera " + outgoing_actor.get_actor_label())
    actor_subsystem.destroy_actor(outgoing_actor)

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    log("done: {} now frames the day stage".format(INSTANCE_LABEL))


main()
