"""Export cooking UI textures to PNG so layout can be measured outside the editor."""

import os

import unreal

OUTPUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), "Exported", "CookingUI")

ASSETS = [
    "/Game/Day/Art/cookingUI/T_CookingUI_Concept",
    "/Game/Day/Art/cookingUI/T_CookingUI_Overlay_01",
    "/Game/Day/Art/cookingUI/T_CookingUI_Overlay_02",
    "/Game/Day/Art/cookingUI/T_CookingUI_Overlay_03",
    "/Game/Day/Art/cookingUI/T_CookingUI_Overlay_04",
]


def export_one(asset_path):
    texture = unreal.load_asset(asset_path)
    if texture is None:
        unreal.log_warning("missing asset {0}".format(asset_path))
        return

    filename = os.path.join(OUTPUT_DIR, "{0}.png".format(asset_path.rsplit("/", 1)[-1]))
    task = unreal.AssetExportTask()
    task.set_editor_property("object", texture)
    task.set_editor_property("filename", filename)
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("replace_identical", True)
    task.set_editor_property("exporter", unreal.TextureExporterPNG())

    if unreal.Exporter.run_asset_export_task(task):
        unreal.log("exported {0}".format(filename))
    else:
        unreal.log_warning("export failed {0}".format(asset_path))


def main():
    if not os.path.isdir(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)
    for asset_path in ASSETS:
        export_one(asset_path)


main()
