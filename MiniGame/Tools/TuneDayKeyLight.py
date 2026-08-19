# Try a key light intensity / colour on the live presenter, for eyeballing exposure in PIE.
#
# Usage: py TuneDayKeyLight.py <intensity> [r g b]

import sys

import unreal


def main():
    args = [arg for arg in sys.argv[1:] if not arg.startswith("-")]
    intensity = float(args[0]) if args else 1.5
    color = tuple(float(value) for value in args[1:4]) if len(args) >= 4 else None

    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world()
    if world is None:
        unreal.log_error("[KeyLight] no PIE world")
        return

    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if "SDayBoardPresenter" not in actor.get_class().get_name():
            continue
        for component in actor.get_components_by_class(unreal.DirectionalLightComponent):
            component.set_intensity(intensity)
            if color:
                component.set_light_color(unreal.LinearColor(color[0], color[1], color[2]))
            unreal.log("[KeyLight] {} intensity={} color={} rot={}".format(
                component.get_name(), intensity, color or "unchanged",
                component.get_world_rotation()))


main()
