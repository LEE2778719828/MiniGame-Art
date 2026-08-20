"""Reparent BP_DayCamera onto ASDayCameraRig so the cookingUI layer solve also runs at edit time.

FitDayArtLayers used to run only from ASDayBoardPresenter::BeginPlay, which left the editor camera
preview showing whatever layer depths happened to be authored last. ASDayCameraRig calls the same
solve from OnConstruction, so the camera Blueprint has to descend from the rig for the preview and
the game view to agree. Component tags, transforms and the SCS survive a reparent untouched, so
this only swaps the parent class.
"""

import unreal


CAMERA_BP_PATH = "/Game/Day/Blueprints/BP_DayCamera"
RIG_CLASS_PATH = "/Script/MiniGame.SDayCameraRig"


def log(message):
    unreal.log("[ReparentDayCamera] " + message)


def fail(message):
    unreal.log_error("[ReparentDayCamera] " + message)
    raise RuntimeError(message)


def derives_from(blueprint, cls):
    """UBlueprint.ParentClass is not exposed to Python, so ask the generated class instead."""
    generated = unreal.BlueprintEditorLibrary.generated_class(blueprint)
    if generated is None:
        return False
    return unreal.MathLibrary.class_is_child_of(generated, cls)


def main():
    blueprint = unreal.load_asset(CAMERA_BP_PATH)
    if blueprint is None:
        fail("missing " + CAMERA_BP_PATH)

    rig_class = unreal.load_class(None, RIG_CLASS_PATH)
    if rig_class is None:
        fail("missing {}; rebuild the MiniGameEditor target first".format(RIG_CLASS_PATH))

    if derives_from(blueprint, rig_class):
        log("already derives from SDayCameraRig; nothing to do")
        return

    log("reparenting " + CAMERA_BP_PATH + " onto SDayCameraRig")
    unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, rig_class)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not unreal.EditorAssetLibrary.save_asset(CAMERA_BP_PATH):
        fail("could not save " + CAMERA_BP_PATH)

    if not derives_from(unreal.load_asset(CAMERA_BP_PATH), rig_class):
        fail("reparent did not take")
    log("done: BP_DayCamera now derives from SDayCameraRig")


main()
