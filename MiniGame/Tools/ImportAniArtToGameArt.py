# Import each FBX independently into /Game/Art/<Name>/
# Avoids shared-skeleton bone mismatches across Mixamo packs.

import unreal
import os

SOURCE_DIR = r"E:\UEProjects\MiniGame\MiniGame\Saved\ImportSource\ani"
ROOT_DEST = "/Game/Art"

FILES = [
    "Slash.fbx",
    "Slash_fast.fbx",
    "Jump_noknife2.fbx",
    "Jump_noknife2_fast.fbx",
    "Sitting.fbx",
    "Knife.fbx",
]


def log(msg: str) -> None:
    unreal.log(f"[ImportAniArt] {msg}")


def import_one(filename: str, as_static: bool = False):
    stem = filename.rsplit(".", 1)[0]
    dest = f"{ROOT_DEST}/{stem}"
    src = os.path.join(SOURCE_DIR, filename)
    if not os.path.isfile(src):
        unreal.log_error(f"[ImportAniArt] missing: {src}")
        return []

    if not unreal.EditorAssetLibrary.does_directory_exist(dest):
        unreal.EditorAssetLibrary.make_directory(dest)

    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = dest
    task.destination_name = stem
    task.automated = True
    task.save = True
    task.replace_existing = True

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = True
    options.import_textures = True
    options.create_physics_asset = False
    options.automated_import_should_detect_type = False

    if as_static:
        options.import_as_skeletal = False
        options.import_animations = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        options.original_import_type = unreal.FBXImportType.FBXIT_STATIC_MESH
    else:
        options.import_as_skeletal = True
        options.import_animations = True
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
        options.original_import_type = unreal.FBXImportType.FBXIT_SKELETAL_MESH

    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = [str(p) for p in task.imported_object_paths]
    log(f"{filename} -> {paths}")
    return paths


def main():
    if not os.path.isdir(SOURCE_DIR):
        unreal.log_error(f"[ImportAniArt] source dir missing: {SOURCE_DIR}")
        return

    if not unreal.EditorAssetLibrary.does_directory_exist(ROOT_DEST):
        unreal.EditorAssetLibrary.make_directory(ROOT_DEST)

    for filename in FILES:
        as_static = filename.lower() == "knife.fbx"
        try:
            import_one(filename, as_static=as_static)
        except Exception as ex:
            unreal.log_error(f"[ImportAniArt] failed {filename}: {ex}")
            # Knife fallback: try skeletal if static fails
            if as_static:
                try:
                    import_one(filename, as_static=False)
                except Exception as ex2:
                    unreal.log_error(f"[ImportAniArt] knife skeletal fallback failed: {ex2}")

    unreal.EditorAssetLibrary.save_directory(ROOT_DEST, only_if_is_dirty=False, recursive=True)
    log("Done")


main()
