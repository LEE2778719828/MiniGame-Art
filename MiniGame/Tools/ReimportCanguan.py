"""Reimport the updated canguan FBX into the Day art directory.

This intentionally imports the FBX as a scene of separate static meshes. Existing
assets with matching names are replaced in place so level references stay valid;
newly added meshes are created beside them.
"""

import os

import unreal


SOURCE_FBX = r"E:\UEProjects\MiniGame\ArtSubmit\Environment\mini_canguan\canguan.fbx"
DESTINATION = "/Game/Day/Art/canguan"


def main():
    if not os.path.isfile(SOURCE_FBX):
        unreal.log_error("[ReimportCanguan] source not found: {}".format(SOURCE_FBX))
        return

    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("original_import_type", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("import_materials", True)
    options.set_editor_property("import_textures", True)

    static_options = options.get_editor_property("static_mesh_import_data")
    static_options.set_editor_property("combine_meshes", False)
    static_options.set_editor_property("generate_lightmap_u_vs", True)
    static_options.set_editor_property("auto_generate_collision", True)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", SOURCE_FBX)
    task.set_editor_property("destination_path", DESTINATION)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", options)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    imported = [str(path) for path in task.get_editor_property("imported_object_paths")]
    unreal.log(
        "[ReimportCanguan] imported {} object(s) from {}: {}".format(
            len(imported), SOURCE_FBX, imported
        )
    )
    unreal.EditorAssetLibrary.save_directory(
        DESTINATION, only_if_is_dirty=False, recursive=True
    )


main()
