# List the materials the restaurant meshes render with, in whichever world is active.

import unreal


def world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    game = editor.get_game_world()
    return game if game is not None else editor.get_editor_world()


def main():
    target = world()
    if target is None:
        unreal.log_error("[Canguan] no world")
        return
    unreal.log("[Canguan] world {}".format(target.get_path_name()))
    for actor in unreal.GameplayStatics.get_all_actors_of_class(target, unreal.StaticMeshActor):
        label = actor.get_actor_label() if hasattr(actor, "get_actor_label") else actor.get_name()
        if "Canguan" not in label and "ENV" not in label:
            continue
        component = actor.static_mesh_component
        mesh = component.static_mesh
        materials = component.get_materials()
        unreal.log("[Canguan] {:<28} mesh={} materials={}".format(
            label,
            mesh.get_name() if mesh else "none",
            [m.get_name() if m else "none" for m in materials]))


main()
