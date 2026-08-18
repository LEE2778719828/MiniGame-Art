# Remove per-FBX duplicate meshes/skeletons after unifying onto Slash_Skeleton.
# All animations now live under /Game/Art/Anims and bind to /Game/Art/Slash/Slash_Skeleton.

import unreal

DIRS = [
    "/Game/Art/Slash_fast",
    "/Game/Art/Jump_noknife2",
    "/Game/Art/Jump_noknife2_fast",
    "/Game/Art/Sitting",
]


def main():
    for directory in DIRS:
        if not unreal.EditorAssetLibrary.does_directory_exist(directory):
            unreal.log(f"[Prune] skip missing {directory}")
            continue
        ok = unreal.EditorAssetLibrary.delete_directory(directory)
        unreal.log(f"[Prune] delete {directory} -> {ok}")

    unreal.log("[Prune] Done")


main()
