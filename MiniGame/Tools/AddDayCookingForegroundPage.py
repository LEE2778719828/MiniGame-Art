"""Give BP_DayCamera the cookingUI overlay page it is missing.

The backdrop already hangs off the camera; the overlay (coins, tally, red rope) is the same kind
of page with the opposite tag, so FitDayArtLayers brackets it in front of the stall instead of
behind it. Depth and scale are left unset here on purpose: the presenter solves both on BeginPlay.
"""

import unreal


CAMERA_BP_PATH = "/Game/Day/Blueprints/BP_DayCamera"
FOREGROUND_WBP_PATH = "/Game/Day/UI/WBP_SDayCookingForeground"
CAMERA_COMPONENT_NAME = "Camera"
PAGE_COMPONENT_NAME = "foreground"
PAGE_TAG = "SDay.Foreground"
# The engine clamps a WidgetComponent's render target to 3840x2160, and the quad is built from the
# render target rather than DrawSize, so the page is the tallest 1440x3200 slice that survives it.
PAGE_SIZE = unreal.IntPoint(972, 2160)


def log(message):
    unreal.log("[DayCookingForeground] " + message)


def fail(message):
    unreal.log_error("[DayCookingForeground] " + message)
    raise RuntimeError(message)


def subobject_nodes(subsystem, blueprint):
    nodes = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is not None:
            nodes[obj.get_name().replace("_GEN_VARIABLE", "")] = (handle, obj)
    return nodes


def main():
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        fail("stop PIE first")

    blueprint = unreal.load_asset(CAMERA_BP_PATH)
    if blueprint is None:
        fail("missing " + CAMERA_BP_PATH)
    page_class = unreal.load_asset(FOREGROUND_WBP_PATH)
    if page_class is None:
        fail("missing " + FOREGROUND_WBP_PATH)

    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    nodes = subobject_nodes(subsystem, blueprint)
    if CAMERA_COMPONENT_NAME not in nodes:
        fail("{} has no component named {}".format(CAMERA_BP_PATH, CAMERA_COMPONENT_NAME))
    camera_handle, _ = nodes[CAMERA_COMPONENT_NAME]

    if PAGE_COMPONENT_NAME in nodes:
        page_handle, page = nodes[PAGE_COMPONENT_NAME]
        if not subsystem.attach_subobject(camera_handle, page_handle):
            fail("could not park the existing {} under the camera".format(PAGE_COMPONENT_NAME))
        log("reusing the existing " + PAGE_COMPONENT_NAME)
    else:
        params = unreal.AddNewSubobjectParams()
        params.set_editor_property("parent_handle", camera_handle)
        params.set_editor_property("new_class", unreal.WidgetComponent)
        params.set_editor_property("blueprint_context", blueprint)
        page_handle, error = subsystem.add_new_subobject(params)
        if error and not error.is_empty():
            fail("adding {} failed: {}".format(PAGE_COMPONENT_NAME, error))
        subsystem.rename_subobject(page_handle, unreal.Text(PAGE_COMPONENT_NAME))
        data = subsystem.k2_find_subobject_data_from_handle(page_handle)
        page = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        log("added " + PAGE_COMPONENT_NAME)

    page.set_editor_property("widget_class", page_class.generated_class())
    page.set_editor_property("space", unreal.WidgetSpace.WORLD)
    page.set_editor_property("draw_size", PAGE_SIZE)
    page.set_editor_property("pivot", unreal.Vector2D(0.5, 0.5))
    page.set_editor_property("draw_at_desired_size", False)
    # The overlay is mostly cut-out, and its edges have to feather into the stall behind it, so it
    # blends rather than alpha-tests like the opaque backdrop does.
    page.set_editor_property("blend_mode", unreal.WidgetBlendMode.TRANSPARENT)
    page.set_editor_property("receive_hardware_input", False)
    # A one-sided quad vanishes whenever the baked facing and the engine's winding disagree, and
    # this page is only ever seen from the camera it hangs off, so cull nothing.
    page.set_editor_property("is_two_sided", True)
    # The quad lives in the component's local YZ plane with its visible face along local +X, so the
    # face has to look back down the camera's view axis: a half turn keeps it upright and unmirrored.
    page.set_editor_property("relative_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0))
    page.set_editor_property("component_tags", [unreal.Name(PAGE_TAG)])
    # The playable HUD takes the clicks; the art must never eat a delivery tap.
    body = page.get_editor_property("body_instance")
    body.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
    page.set_editor_property("body_instance", body)
    page.set_editor_property("generate_overlap_events", False)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_asset(CAMERA_BP_PATH)
    log("done: the overlay page rides the camera as " + PAGE_COMPONENT_NAME)


main()
