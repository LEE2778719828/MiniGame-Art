"""Align animated restaurant boxes to the existing tagged interaction anchors."""

import math

import unreal


BLUEPRINT_PATH = "/Game/Day/Blueprints/BP_SDayCanguan"
ANIMATED_NAMES = [f"BoxAnim_{index}" for index in range(5)]
BIN_TAGS = [f"SDay.Bin.{index}" for index in range(5)]


def fail(message):
    unreal.log_error("[DayAnimatedBoxes] " + message)
    raise RuntimeError(message)


def component_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    templates = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if isinstance(obj, unreal.SceneComponent):
            templates[obj.get_name().replace("_GEN_VARIABLE", "")] = obj
    return templates


def find_anchor(templates, tag):
    for component in templates.values():
        tags = [str(value) for value in component.get_editor_property("component_tags")]
        if tag in tags:
            return component
    fail("Missing component tagged " + tag)


def component_multiply(a, b):
    return unreal.Vector(a.x * b.x, a.y * b.y, a.z * b.z)


def component_divide(a, b):
    return unreal.Vector(a.x / b.x, a.y / b.y, a.z / b.z)


def configure_pair(anchor, animated, index):
    if not isinstance(anchor, unreal.StaticMeshComponent):
        fail("{} anchor is not a StaticMeshComponent".format(BIN_TAGS[index]))
    if not isinstance(animated, unreal.SkeletalMeshComponent):
        fail("{} is not a SkeletalMeshComponent".format(ANIMATED_NAMES[index]))

    rotation = anchor.get_editor_property("relative_rotation")
    if abs(rotation.pitch) > 0.001 or abs(rotation.yaw) > 0.001 or abs(rotation.roll) > 0.001:
        fail("{} has a non-zero rotation; automatic bounds alignment would be unsafe".format(
            anchor.get_name()))

    static_mesh = anchor.get_editor_property("static_mesh")
    skeletal_mesh = animated.get_editor_property("skeletal_mesh_asset")
    if static_mesh is None or skeletal_mesh is None:
        fail("Missing mesh on pair {}".format(index))

    old_bounds = static_mesh.get_bounds()
    new_bounds = skeletal_mesh.get_bounds()
    old_scale = anchor.get_editor_property("relative_scale3d")
    ratios = component_divide(
        component_multiply(old_bounds.box_extent, old_scale),
        new_bounds.box_extent)
    uniform_scale = math.pow(ratios.x * ratios.y * ratios.z, 1.0 / 3.0)
    new_scale = unreal.Vector(uniform_scale, uniform_scale, uniform_scale)

    old_center = (
        anchor.get_editor_property("relative_location")
        + component_multiply(old_bounds.origin, old_scale))
    new_location = old_center - component_multiply(new_bounds.origin, new_scale)

    animated.set_editor_property("relative_location", new_location)
    animated.set_editor_property("relative_rotation", rotation)
    animated.set_editor_property("relative_scale3d", new_scale)
    animated.set_editor_property("generate_overlap_events", False)
    animated.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    animated.set_material(0, anchor.get_material(0))

    animation_data = animated.get_editor_property("animation_data")
    animation_data.set_editor_property("saved_looping", False)
    animation_data.set_editor_property("saved_playing", False)
    animation_data.set_editor_property("saved_position", 0.0)
    animation_data.set_editor_property("saved_play_rate", 1.0)
    animated.set_editor_property("animation_data", animation_data)

    anchor.set_editor_property("visible", False)
    anchor.set_editor_property("hidden_in_game", True)

    unreal.log(
        "[DayAnimatedBoxes] {} -> {} location={} scale={:.4f} material={}".format(
            anchor.get_name(),
            animated.get_name(),
            new_location,
            uniform_scale,
            anchor.get_material(0).get_path_name() if anchor.get_material(0) else "None"))


def main():
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        fail("Stop PIE before configuring the Blueprint")

    blueprint = unreal.load_asset(BLUEPRINT_PATH)
    if blueprint is None:
        fail("Missing " + BLUEPRINT_PATH)

    templates = component_templates(blueprint)
    for index, (animated_name, tag) in enumerate(zip(ANIMATED_NAMES, BIN_TAGS)):
        if animated_name not in templates:
            fail("Missing component " + animated_name)
        configure_pair(find_anchor(templates, tag), templates[animated_name], index)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not unreal.EditorAssetLibrary.save_asset(BLUEPRINT_PATH):
        fail("Could not save " + BLUEPRINT_PATH)
    unreal.log("[DayAnimatedBoxes] Done")


main()
