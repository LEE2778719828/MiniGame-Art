"""Compile and save the cooking foreground page after its widget tree was edited."""

import unreal

ASSET_PATH = "/Game/Day/UI/WBP_SDayCookingForeground"


def main():
    blueprint = unreal.load_asset(ASSET_PATH)
    if blueprint is None:
        unreal.log_error("missing asset {0}".format(ASSET_PATH))
        return

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if unreal.EditorAssetLibrary.save_asset(ASSET_PATH, only_if_is_dirty=False):
        unreal.log("saved {0}".format(ASSET_PATH))
    else:
        unreal.log_error("save failed {0}".format(ASSET_PATH))


main()
