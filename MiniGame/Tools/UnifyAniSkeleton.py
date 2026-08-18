# Re-import animations from the ani pack onto a single master skeleton.
# Master = /Game/Art/Slash/Slash_Skeleton (47 bones, common subset of all rigs).

import unreal
import os

SOURCE_DIR = r"E:\UEProjects\MiniGame\MiniGame\Saved\ImportSource\ani"
MASTER_SKELETON = "/Game/Art/Slash/Slash_Skeleton"
DEST_ROOT = "/Game/Art/Anims"

FILES = [
    "Slash_fast.fbx",
    "Jump_noknife2.fbx",
    "Jump_noknife2_fast.fbx",
    "Sitting.fbx",
]


def log(msg: str) -> None:
    unreal.log(f"[UnifySkeleton] {msg}")


def import_anims(filename: str, skeleton):
    stem = filename.rsplit(".", 1)[0]
    src = os.path.join(SOURCE_DIR, filename)
    dest = f"{DEST_ROOT}/{stem}"
    if not os.path.isfile(src):
        unreal.log_error(f"[UnifySkeleton] missing: {src}")
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
    options.import_mesh = False
    options.import_as_skeletal = True
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.create_physics_asset = False
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.original_import_type = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.skeleton = skeleton

    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = [str(p) for p in task.imported_object_paths]
    log(f"{filename} -> {len(paths)} anim(s): {paths}")
    return paths


def main():
    skeleton = unreal.EditorAssetLibrary.load_asset(MASTER_SKELETON)
    if skeleton is None:
        unreal.log_error(f"[UnifySkeleton] master skeleton not found: {MASTER_SKELETON}")
        return

    if not unreal.EditorAssetLibrary.does_directory_exist(DEST_ROOT):
        unreal.EditorAssetLibrary.make_directory(DEST_ROOT)

    for filename in FILES:
        try:
            import_anims(filename, skeleton)
        except Exception as ex:
            unreal.log_error(f"[UnifySkeleton] failed {filename}: {ex}")

    unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=False, recursive=True)
    log("Done")


main()
