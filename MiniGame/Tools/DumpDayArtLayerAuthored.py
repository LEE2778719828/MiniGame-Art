"""Report the authored cookingUI layer transforms, i.e. what BP_DayCamera stores before any solve.

FitDayArtLayers overwrites the layer transforms whenever it runs, so a level instance only ever
shows the solved result. The Blueprint's own component templates are untouched by that solve, which
makes them the reference for deciding whether the runtime re-fit changes the picture or merely
recomputes what was already authored.
"""

import unreal


CAMERA_BP_PATH = "/Game/Day/Blueprints/BP_DayCamera"
LAYER_TAGS = ("SDay.Backdrop", "SDay.Foreground")
CAMERA_TAG = "SDayCamera"
REPORT_NAME = "DayArtLayerAuthored.txt"


def component_tags(component):
    return [str(tag) for tag in component.get_editor_property("component_tags")]


def subobject_templates(blueprint):
    """The Blueprint's SCS component templates, keyed by variable name."""
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    templates = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is not None:
            templates[obj.get_name().replace("_GEN_VARIABLE", "")] = obj
    return templates


def main():
    lines = []

    def report(message):
        lines.append(message)
        unreal.log("[DayArtLayerAuthored] " + message)

    blueprint = unreal.load_asset(CAMERA_BP_PATH)
    if blueprint is None:
        report("missing " + CAMERA_BP_PATH)
    else:
        for name, template in sorted(subobject_templates(blueprint).items()):
            if not isinstance(template, unreal.SceneComponent):
                continue
            tags = component_tags(template)
            is_layer = any(tag in tags for tag in LAYER_TAGS)
            if not is_layer and CAMERA_TAG not in tags:
                continue
            location = template.get_editor_property("relative_location")
            scale = template.get_editor_property("relative_scale3d")
            entry = "{:<14} depth={:>10.2f} scale=({:.6f}, {:.6f}) tags={}".format(
                name, location.x, scale.x, scale.y, ",".join(tags))
            if isinstance(template, unreal.WidgetComponent):
                draw = template.get_editor_property("draw_size")
                entry += " draw=({}, {})".format(draw.x, draw.y)
            report(entry)

    path = unreal.Paths.combine([unreal.Paths.project_saved_dir(), REPORT_NAME])
    with open(unreal.Paths.convert_relative_path_to_full(path), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")


main()
