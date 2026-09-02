"""
Compare two skinned FBX rigs before importing the second one against the first one's skeleton.

Reusing one UE skeleton across exports only works if the bone hierarchy and the bind pose
agree. Bone-count differences alone are not fatal -- this art pack keeps appending leaf
terminator bones ("_end", "_end_end") on every re-export -- but a shifted bind pose is,
because existing clips would then play offset and nothing reports it at import time.

Usage:
    python Tools/CompareFbxRigs.py <reference.fbx> <candidate.fbx>
"""

import os
import sys

from InspectFbxStructure import MAGIC, Reader, object_name, object_subtype, read_node

# Bind poses are authored in centimetres here; anything under this is export round-off.
POSE_TOLERANCE = 0.01


def load_roots(path):
    with open(path, "rb") as handle:
        blob = handle.read()
    if not blob.startswith(MAGIC):
        raise ValueError("{} is not a binary FBX".format(path))

    reader = Reader(blob)
    reader.pos = len(MAGIC) + 2
    reader.wide = reader.u32() >= 7500

    roots = []
    while True:
        node = read_node(reader)
        if node is None:
            break
        roots.append(node)
    return roots


def property70(model, key):
    """Read a Properties70 entry, e.g. "Lcl Translation" -> (x, y, z)."""
    props = model.child("Properties70")
    if props is None:
        return None
    for entry in props.kids("P"):
        if entry.props and entry.props[0] == key:
            numbers = [p for p in entry.props if isinstance(p, float)]
            return tuple(numbers) if numbers else None
    return None


def bone_table(path):
    """LimbNode name -> local translation / rotation offsets that define the bind pose."""
    roots = load_roots(path)
    objects = None
    connections = None
    for node in roots:
        if node.name == "Objects":
            objects = node
        elif node.name == "Connections":
            connections = node

    models = [n for n in objects.children if n.name == "Model"]
    by_id = {n.props[0]: n for n in models}
    limbs = {n.props[0]: n for n in models if object_subtype(n) == "LimbNode"}

    # A bone appears as the child end of two different link kinds: bone -> parent bone, and
    # bone -> skin Cluster (the deformer owns the bones it influences). Only the former is a
    # hierarchy edge, so links whose parent is not itself a Model must be ignored, or every
    # skinned bone looks parentless.
    parent_of = {}
    if connections is not None:
        for link in connections.kids("C"):
            p = link.props
            if len(p) >= 3 and p[0] == "OO" and p[1] in limbs and p[2] in by_id:
                parent_of[p[1]] = p[2]

    table = {}
    for bone_id, node in limbs.items():
        name = object_name(node)
        parent_id = parent_of.get(bone_id)
        parent = "<root>"
        if parent_id in by_id:
            parent = object_name(by_id[parent_id])
        table[name] = {
            "parent": parent,
            "translation": property70(node, "Lcl Translation") or (0.0, 0.0, 0.0),
            "prerotation": property70(node, "PreRotation") or (0.0, 0.0, 0.0),
        }
    return table


def bind_table(path):
    """
    Bone name -> global bind translation taken from the skin clusters.

    This, not the Model "Lcl Translation" property, is what the skinning actually resolves
    against: each Cluster stores TransformLink, the world transform the influenced bone had
    when the mesh was bound. A file that carries animation may leave Lcl Translation sitting
    on some posed frame, so comparing bind matrices is the only apples-to-apples check.
    """
    roots = load_roots(path)
    objects = None
    connections = None
    for node in roots:
        if node.name == "Objects":
            objects = node
        elif node.name == "Connections":
            connections = node

    models = {n.props[0]: n for n in objects.children if n.name == "Model"}
    clusters = {n.props[0]: n for n in objects.children
                if n.name == "Deformer" and object_subtype(n) == "Cluster"}

    bone_of_cluster = {}
    if connections is not None:
        for link in connections.kids("C"):
            p = link.props
            if len(p) >= 3 and p[0] == "OO" and p[1] in models and p[2] in clusters:
                bone_of_cluster[p[2]] = object_name(models[p[1]])

    table = {}
    for cluster_id, cluster in clusters.items():
        name = bone_of_cluster.get(cluster_id)
        link = cluster.child("TransformLink")
        if name is None or link is None or not link.props:
            continue
        matrix = link.props[0].values()
        if len(matrix) == 16:
            # Column-major 4x4; the translation sits in the last row.
            table.setdefault(name, (matrix[12], matrix[13], matrix[14]))
    return table


def worst_delta(a, b):
    return max(abs(x - y) for x, y in zip(a, b)) if len(a) == len(b) else float("inf")


def compare(reference_path, candidate_path):
    reference = bone_table(reference_path)
    candidate = bone_table(candidate_path)

    print("reference: {}  ({} bones)".format(os.path.basename(reference_path), len(reference)))
    print("candidate: {}  ({} bones)".format(os.path.basename(candidate_path), len(candidate)))

    missing = sorted(set(reference) - set(candidate))
    extra = sorted(set(candidate) - set(reference))
    shared = sorted(set(reference) & set(candidate))

    print("\nshared bones: {}".format(len(shared)))
    print("missing from candidate: {}  {}".format(len(missing), missing))
    print("extra in candidate:     {}".format(len(extra)))
    for i in range(0, len(extra), 3):
        print("    " + "  ".join("{:<32}".format(name) for name in extra[i:i + 3]))

    # Leaf terminators carry no skin weights, so they are safe to merge into the skeleton.
    non_leaf_extra = [n for n in extra if not n.endswith("_end")]
    reparented = [n for n in shared if reference[n]["parent"] != candidate[n]["parent"]]

    pose_drift = []
    for name in shared:
        move = worst_delta(reference[name]["translation"], candidate[name]["translation"])
        turn = worst_delta(reference[name]["prerotation"], candidate[name]["prerotation"])
        if max(move, turn) > POSE_TOLERANCE:
            pose_drift.append((max(move, turn), name, move, turn))
    pose_drift.sort(reverse=True)

    print("\nhierarchy changes on shared bones: {}".format(len(reparented)))
    for name in reparented[:10]:
        print("    {:<24} {} -> {}".format(
            name, reference[name]["parent"], candidate[name]["parent"]))

    print("node-property drift over {} cm/deg: {} bone(s)".format(
        POSE_TOLERANCE, len(pose_drift)))
    if pose_drift:
        print("    {:<24} {:>9} {:>9}   {:<26} {}".format(
            "bone", "move cm", "turn deg", "reference translation", "candidate translation"))
    for _, name, move, turn in pose_drift[:10]:
        print("    {:<24} {:>9.4f} {:>9.4f}   {:<26} {}".format(
            name, move, turn,
            "({:.3f}, {:.3f}, {:.3f})".format(*reference[name]["translation"]),
            "({:.3f}, {:.3f}, {:.3f})".format(*candidate[name]["translation"])))

    reference_bind = bind_table(reference_path)
    candidate_bind = bind_table(candidate_path)
    bind_shared = sorted(set(reference_bind) & set(candidate_bind))
    bind_drift = []
    for name in bind_shared:
        drift = worst_delta(reference_bind[name], candidate_bind[name])
        if drift > POSE_TOLERANCE:
            bind_drift.append((drift, name))
    bind_drift.sort(reverse=True)

    print("\nskin bind matrices compared: {} bone(s)".format(len(bind_shared)))
    print("bind drift over {} cm: {} bone(s)".format(POSE_TOLERANCE, len(bind_drift)))
    for drift, name in bind_drift[:10]:
        print("    {:<24} {:>9.4f}   {:<26} {}".format(
            name, drift,
            "({:.3f}, {:.3f}, {:.3f})".format(*reference_bind[name]),
            "({:.3f}, {:.3f}, {:.3f})".format(*candidate_bind[name])))

    blockers = []
    if missing:
        blockers.append("{} bone(s) present in the reference are absent".format(len(missing)))
    if non_leaf_extra:
        blockers.append("{} extra non-leaf bone(s): {}".format(
            len(non_leaf_extra), non_leaf_extra))
    if reparented:
        blockers.append("{} bone(s) changed parent".format(len(reparented)))
    if bind_drift:
        blockers.append("{} bone(s) moved in the skin bind pose".format(len(bind_drift)))

    print("\n" + "=" * 70)
    if blockers:
        print("VERDICT: NOT a drop-in replacement")
        for item in blockers:
            print("  - " + item)
        return 1

    print("VERDICT: drop-in. Same hierarchy, same bind pose;")
    print("         {} extra leaf terminator bone(s) merge harmlessly.".format(len(extra)))
    return 0


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    return compare(argv[0], argv[1])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
