"""Restore Day_Camera cookingUI layers to the Blueprint's authored transforms.

FitDayArtLayers used to overwrite the instance at edit time and at BeginPlay. The solve is no
longer invoked, but the last solved depths remain on the placed actor until they are copied back
from BP_DayCamera's component templates.
"""

import unreal


CAMERA_BP_PATH = "/Game/Day/Blueprints/BP_DayCamera"
LAYER_NAMES = ("background", "foreground")


def log(message):
    unreal.log("[RestoreLayers] " + message)


def fail(message):
    unreal.log_error("[RestoreLayers] " + message)
    raise RuntimeError(message)


def blueprint_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    templates = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is not None:
            templates[obj.get_name().replace("_GEN_VARIABLE", "")] = obj
    return templates


def main():
    blueprint = unreal.load_asset(CAMERA_BP_PATH)
    if blueprint is None:
        fail("missing " + CAMERA_BP_PATH)
    templates = blueprint_templates(blueprint)

    actor = None
    for candidate in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if candidate.get_actor_label() == "Day_Camera":
            actor = candidate
            break
    if actor is None:
        fail("no Day_Camera in the current level")

    components = {component.get_name(): component
                  for component in actor.get_components_by_class(unreal.SceneComponent)}

    for name in LAYER_NAMES:
        template = templates.get(name)
        component = components.get(name)
        if template is None or component is None:
            fail("missing layer " + name)
        location = template.get_editor_property("relative_location")
        rotation = template.get_editor_property("relative_rotation")
        scale = template.get_editor_property("relative_scale3d")
        component.set_relative_location_and_rotation(location, rotation, False, False)
        component.set_relative_scale3d(scale)
        got = component.get_relative_transform()
        log("{} restored depth={:.2f} scale=({:.6f}, {:.6f}) now depth={:.2f} scale=({:.6f}, {:.6f})".format(
            name, location.x, scale.x, scale.y, got.translation.x, got.scale3d.x, got.scale3d.y))

    log("left the level dirty; save from the editor if the preview looks right")


main()
