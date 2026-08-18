def main():
    import unreal
    import os

    source = r"E:\UEProjects\MiniGame\MiniGame\Saved\ImportSource\ani\Slash.fbx"
    dest = "/Game/Art/Slash"

    if not os.path.isfile(source):
        unreal.log_error(f"[ImportSlash] missing: {source}")
        return

    if not unreal.EditorAssetLibrary.does_directory_exist(dest):
        unreal.EditorAssetLibrary.make_directory(dest)

    task = unreal.AssetImportTask()
    task.filename = source
    task.destination_path = dest
    task.destination_name = "Slash"
    task.automated = True
    task.save = True
    task.replace_existing = True

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = True
    options.import_textures = True
    options.create_physics_asset = False
    options.automated_import_should_detect_type = False
    options.import_as_skeletal = True
    options.import_animations = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.original_import_type = unreal.FBXImportType.FBXIT_SKELETAL_MESH

    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.log(f"[ImportSlash] -> {[str(p) for p in task.imported_object_paths]}")
    unreal.EditorAssetLibrary.save_directory(dest, only_if_is_dirty=False, recursive=True)


main()
