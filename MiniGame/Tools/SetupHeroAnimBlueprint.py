# Fills in the parts of the hero animation setup that have no pins and therefore cannot be reached
# through graph editing: the sequence player's clip and loop flag.
#
# Action clips are not wrapped as montage assets. PlayHeroAction feeds Slash / Jump_noknife2 into
# DefaultSlot via PlaySlotAnimationAsDynamicMontage with blend in/out of 0, so the Contact / Land
# notifies stay aligned with AttackAnchorMs / JumpAnchorMs.
#
# Run from the editor: py "D:/myProject/MiniGame/MiniGame/Tools/SetupHeroAnimBlueprint.py"
# (the console resolves relative paths against the engine binaries directory, not the project)

import unreal

ABP_PATH = "/Game/Night/Character/ABP_NightHero"
IDLE_CLIP = "/Game/Night/Character/Anims/Sitting"


def log(msg):
    unreal.log_warning("[SetupHeroABP] " + msg)


def try_set(obj, candidates, value):
    """Property names drift between engine versions, so probe instead of assuming one."""
    for name in candidates:
        try:
            obj.set_editor_property(name, value)
            return name
        except Exception:
            continue
    return None


def sequence_player_nodes(blueprint):
    """UBlueprint's graph containers are not reflected to Python; AnimBlueprint's own accessor is."""
    found = []
    for graph in blueprint.get_animation_graphs():
        found.extend(graph.get_graph_nodes_of_class(unreal.AnimGraphNode_SequencePlayer, True))
    return found


def configure_sequence_player(blueprint):
    clip = unreal.EditorAssetLibrary.load_asset(IDLE_CLIP)
    if not clip:
        log("idle clip missing: " + IDLE_CLIP)
        return False

    for graph_node in sequence_player_nodes(blueprint):
        # The FAnimNode_* struct comes back by value, so it has to be written back after editing.
        anim_node = graph_node.get_editor_property("node")
        used_seq = try_set(anim_node, ["sequence"], clip)
        used_loop = try_set(anim_node, ["loop_animation", "b_loop_animation", "loop"], True)
        graph_node.set_editor_property("node", anim_node)

        log("sequence player: clip via '{}', loop via '{}'".format(used_seq, used_loop))
        return used_seq is not None

    log("no sequence player found in AnimGraph")
    return False


blueprint = unreal.EditorAssetLibrary.load_asset(ABP_PATH)
if not blueprint:
    log("animation blueprint missing: " + ABP_PATH)
else:
    if configure_sequence_player(blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)

log("done")
