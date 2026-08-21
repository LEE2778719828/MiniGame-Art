"""Import the five animated ingredient boxes into the Day restaurant art folder."""

import os

import unreal


SOURCE_DIR = r"E:\UEProjects\MiniGame\ArtSubmit\Animation\vegetableb_box"
DESTINATION = "/Game/Day/Art/canguan/animation"
FILES = [f"box{index}.fbx" for index in range(1, 6)]


def log(message):
    unreal.log("[VegetableBoxImport] " + message)


def fail(message):
    unreal.log_error("[VegetableBoxImport] " + message)
    raise RuntimeError(message)


def configure_scene_conversion(import_data):
    if import_data is None:
        return
    import_data.set_editor_property("convert_scene", True)
    import_data.set_editor_property("force_front_x_axis", False)
    import_data.set_editor_property("convert_scene_unit", False)


def make_options():
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("import_animations", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("create_physics_asset", False)
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property(
        "mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    options.set_editor_property(
        "original_import_type", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    configure_scene_conversion(options.get_editor_property("skeletal_mesh_import_data"))
    configure_scene_conversion(options.get_editor_property("anim_sequence_import_data"))
    return options


def make_task(filename):
    source = os.path.join(SOURCE_DIR, filename)
    if not os.path.isfile(source):
        fail("Missing source file: " + source)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", DESTINATION)
    task.set_editor_property("destination_name", os.path.splitext(filename)[0])
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("options", make_options())
    return task


def main():
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        fail("Stop PIE before importing")

    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)

    tasks = [make_task(filename) for filename in FILES]
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    imported = []
    for task in tasks:
        paths = [str(path) for path in task.get_editor_property("imported_object_paths")]
        if not paths:
            fail("No assets imported from " + task.get_editor_property("filename"))
        imported.extend(paths)
        log(os.path.basename(task.get_editor_property("filename")) + " -> " + ", ".join(paths))

    unreal.EditorAssetLibrary.save_directory(
        DESTINATION, only_if_is_dirty=False, recursive=True)
    log("Done: imported {} assets".format(len(imported)))


main()
