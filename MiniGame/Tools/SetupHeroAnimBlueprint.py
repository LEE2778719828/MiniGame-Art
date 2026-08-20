# Fills in the parts of the hero animation setup that have no pins and therefore cannot be reached
# through graph editing: the sequence player's clip, and the montage blend timings.
#
# Blend timing matters more than it looks. Both action clips are only 367ms long, while a montage
# defaults to a 250ms blend in and a 250ms blend out. At those defaults the slash would still be
# fading in when it reaches its 179ms contact frame, and the blend out (which starts at
# length - blendOut) would already have begun at 117ms, before contact. Zeroing both keeps the
# clips playing exactly as authored, so the Contact / Land notifies stay aligned with the
# AttackAnchorMs / JumpAnchorMs values on BP_NightHero.
#
# Run from the editor: py "D:/myProject/MiniGame/MiniGame/Tools/SetupHeroAnimBlueprint.py"
# (the console resolves relative paths against the engine binaries directory, not the project)

import unreal

ABP_PATH = "/Game/Night/Character/ABP_NightHero"
IDLE_CLIP = "/Game/Night/Character/Anims/Sitting"
MONTAGES = [
    "/Game/Night/Character/Anims/AM_Slash",
    "/Game/Night/Character/Anims/AM_Jump",
]


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


def zero_montage_blends():
    for path in MONTAGES:
        montage = unreal.EditorAssetLibrary.load_asset(path)
        if not montage:
            log("montage missing: " + path)
            continue
        for prop in ("blend_in", "blend_out"):
            blend = montage.get_editor_property(prop)
            try_set(blend, ["blend_time"], 0.0)
            montage.set_editor_property(prop, blend)
        log("{}: blend in/out zeroed, length {:.3f}s".format(
            montage.get_name(), montage.get_play_length()))
        unreal.EditorAssetLibrary.save_loaded_asset(montage, False)


blueprint = unreal.EditorAssetLibrary.load_asset(ABP_PATH)
if not blueprint:
    log("animation blueprint missing: " + ABP_PATH)
else:
    if configure_sequence_player(blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)

zero_montage_blends()
log("done")
