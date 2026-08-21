"""Log authored restaurant box component state without modifying the Blueprint."""

import unreal


BLUEPRINT_PATH = "/Game/Day/Blueprints/BP_SDayCanguan"


def component_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    templates = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if isinstance(obj, unreal.SceneComponent):
            templates[obj.get_name().replace("_GEN_VARIABLE", "")] = obj
    return templates


def asset_name(value):
    return value.get_path_name() if value is not None else "None"


def bounds_text(mesh):
    if mesh is None:
        return "None"
    bounds = mesh.get_bounds()
    return "origin={} extent={}".format(bounds.origin, bounds.box_extent)


blueprint = unreal.load_asset(BLUEPRINT_PATH)
if blueprint is None:
    raise RuntimeError("Missing " + BLUEPRINT_PATH)

for name, component in sorted(component_templates(blueprint).items()):
    tags = [str(tag) for tag in component.get_editor_property("component_tags")]
    if not name.startswith("box") and not name.startswith("BoxAnim") and not any(
            tag.startswith("SDay.Bin.") for tag in tags):
        continue

    parent = component.get_attach_parent()
    parent_name = parent.get_name().replace("_GEN_VARIABLE", "") if parent else "None"
    mesh = None
    animation = None
    if isinstance(component, unreal.StaticMeshComponent):
        mesh = component.get_editor_property("static_mesh")
    elif isinstance(component, unreal.SkeletalMeshComponent):
        mesh = component.get_editor_property("skeletal_mesh_asset")
        animation_data = component.get_editor_property("animation_data")
        animation = animation_data.get_editor_property("anim_to_play")

    unreal.log(
        "[DayCanguanBoxes] {} class={} parent={} location={} rotation={} scale={} "
        "visible={} hidden={} tags={} mesh={} animation={}".format(
            name,
            component.get_class().get_name(),
            parent_name,
            component.get_editor_property("relative_location"),
            component.get_editor_property("relative_rotation"),
            component.get_editor_property("relative_scale3d"),
            component.get_editor_property("visible"),
            component.get_editor_property("hidden_in_game"),
            tags,
            asset_name(mesh),
            asset_name(animation)))
    if mesh is not None:
        unreal.log("[DayCanguanBoxes] {} bounds {}".format(name, bounds_text(mesh)))
