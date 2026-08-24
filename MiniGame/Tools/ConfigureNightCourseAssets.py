"""Configure the canonical Night foe map and editor-only Atom previews."""

import os

import unreal


CONFIG_PATH = "/Game/Night/Course/Config/DA_Course"
FOE_PATHS = {
    "M01": "/Game/Night/Course/Blueprints/BP_NightFoeM01",
    "M02": "/Game/Night/Course/Blueprints/BP_NightFoeM02",
    "M03": "/Game/Night/Course/Blueprints/BP_NightFoeM03",
    "M04": "/Game/Night/Course/Blueprints/BP_NightFoeM04",
    "M05": "/Game/Night/Course/Blueprints/BP_NightFoeM05",
}
ATOM_PREVIEW_POINTS = {
    "/Game/Night/Course/Atoms/BP_NightAtom_Art_A": (1, 2, 3),
    "/Game/Night/Course/Atoms/BP_NightAtom_Art_B": (1, 2, 3),
    "/Game/Night/Course/Atoms/BP_NightAtom_Art_C": (1, 2, 3),
}


def load_asset(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError("Missing asset: {}".format(path))
    return asset


def component_templates(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    templates = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if isinstance(obj, unreal.SceneComponent):
            name = obj.get_name().replace("_GEN_VARIABLE", "")
            templates[name] = obj
    return templates


def make_foe_map():
    existing_config = load_asset(CONFIG_PATH)
    existing_default = existing_config.get_editor_property("DefaultFoeId")
    foe_enum = type(existing_default)
    foe_map = {}
    for foe_name, path in FOE_PATHS.items():
        blueprint = load_asset(path)
        generated_class = blueprint.generated_class()
        if generated_class is None:
            raise RuntimeError("Blueprint has no generated class: {}".format(path))
        if not unreal.MathLibrary.class_is_child_of(
                generated_class, unreal.NightCourseStoneActor):
            raise RuntimeError(
                "{} is not a NightCourseStoneActor Blueprint".format(path))
        foe_map[getattr(foe_enum, foe_name)] = generated_class
    return foe_map


def make_packages_writable():
    for path in [CONFIG_PATH] + list(FOE_PATHS.values()) + list(
            ATOM_PREVIEW_POINTS.keys()):
        relative_path = path.replace("/Game/", "") + ".uasset"
        package_filename = os.path.join(
            unreal.Paths.project_content_dir(),
            relative_path.replace("/", os.sep))
        if os.path.exists(package_filename):
            os.chmod(package_filename, 0o666)


def main():
    make_packages_writable()
    foe_map = make_foe_map()

    config = load_asset(CONFIG_PATH)
    config.set_editor_property("FoeActorMap", foe_map)
    config.modify()

    foe_preview_class = foe_map[getattr(type(config.get_editor_property("DefaultFoeId")), "M01")]
    for atom_path, indexes in ATOM_PREVIEW_POINTS.items():
        blueprint = load_asset(atom_path)
        templates = component_templates(blueprint)
        for index in indexes:
            name = "LandingPoint_{:02d}".format(index)
            component = templates.get(name)
            if component is None:
                raise RuntimeError("{} is missing {}".format(atom_path, name))
            component.set_editor_property(
                "LandingVisualPrefab", foe_preview_class)
            component.modify()
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        if not unreal.EditorAssetLibrary.save_asset(atom_path):
            raise RuntimeError("Could not save {}".format(atom_path))

    if not unreal.EditorAssetLibrary.save_asset(CONFIG_PATH):
        raise RuntimeError("Could not save {}".format(CONFIG_PATH))
    unreal.log("[NightCourseAssets] Configured FoeActorMap and Atom previews")


main()
