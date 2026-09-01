# Verify the hatless hero swap did not break the shared rig.
#
# Importing SK_Hero_NoHat merged 15 leaf terminator bones into SK_Hero_Skeleton, which marked
# the dependent AnimSequences dirty and re-saved them. That coupling is the risky part of the
# swap: a bone-tree change can leave clips bound to the wrong rig, playing lying down, or
# holding no bone tracks at all -- none of which reports an error at load time.
#
# The three checks below mirror the acceptance criteria already used by Tools/ImportHeroRig.py.
#
# Run from the editor console:  py "<project>/Tools/ValidateHeroNoHat.py"

import unreal

ROOT = "/Game/Night/Character"
SKELETON_PATH = ROOT + "/SK_Hero_Skeleton"
MESHES = [ROOT + "/SK_Hero", ROOT + "/SK_Hero_NoHat"]
ANIM_DIR = ROOT + "/Anims"

# Same thresholds as ImportHeroRig.py: an empty clip spans 0.00 on every channel, the shortest
# real clip in this pack spans 8.4 in translation alone.
MIN_MOTION_SPAN = 0.5
MOTION_BONES = ("Hips", "RightHand", "LeftFoot", "Spine2")
MOTION_SAMPLES = (0.0, 0.15, 0.3, 0.45, 0.6, 0.75, 0.9, 0.99)

TAG = "[ValidateHeroNoHat]"


def log(msg):
    unreal.log("{} {}".format(TAG, msg))


def fail(msg):
    unreal.log_error("{} {}".format(TAG, msg))


def package_path(object_path):
    return str(object_path).split(".")[0]


def asset_class_name(asset_data):
    try:
        return str(asset_data.asset_class_path.asset_name)
    except Exception:
        return str(asset_data.asset_class)


def assets_of_class(directory, class_name):
    found = []
    if not unreal.EditorAssetLibrary.does_directory_exist(directory):
        return found
    for object_path in unreal.EditorAssetLibrary.list_assets(
        directory, recursive=True, include_folder=False
    ):
        data = unreal.EditorAssetLibrary.find_asset_data(object_path)
        if data and asset_class_name(data) == class_name:
            found.append(package_path(object_path))
    return found


def clip_motion_span(anim):
    """Widest movement of any sampled bone; 0 means the clip is a frozen pose."""
    length = anim.get_editor_property("sequence_length")
    if length <= 0.0:
        return 0.0

    widest = 0.0
    for bone in MOTION_BONES:
        channels = [[] for _ in range(6)]
        for fraction in MOTION_SAMPLES:
            try:
                pose = unreal.AnimationLibrary.get_bone_pose_for_time(
                    anim, bone, length * fraction, False)
            except Exception:
                channels = None
                break
            rotation = pose.rotation.rotator()
            for index, value in enumerate((pose.translation.x, pose.translation.y,
                                           pose.translation.z, rotation.roll,
                                           rotation.pitch, rotation.yaw)):
                channels[index].append(value)
        if channels is None:
            continue
        widest = max(widest, *(max(v) - min(v) for v in channels))
    return widest


def main():
    problems = []

    skeleton = unreal.EditorAssetLibrary.load_asset(SKELETON_PATH)
    if skeleton is None:
        fail("skeleton missing at {}".format(SKELETON_PATH))
        return
    bones = skeleton.get_editor_property("bone_tree")
    log("skeleton {} bone tree entries: {}".format(skeleton.get_name(), len(bones) if bones else "?"))

    # Both meshes must still resolve against the one shared skeleton, or animations that play on
    # one will refuse to play on the other.
    for path in MESHES:
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if mesh is None:
            problems.append("mesh missing: {}".format(path))
            continue
        bound = mesh.get_editor_property("skeleton")
        bound_path = package_path(bound.get_path_name()) if bound else "None"
        ok = bound_path == SKELETON_PATH
        try:
            subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
            verts = subsystem.get_num_verts(mesh, 0)
        except Exception:
            verts = -1
        log("{}: verts={} skeleton={} {}".format(
            path.rsplit("/", 1)[-1], verts, bound_path, "OK" if ok else "MISMATCH"))
        if not ok:
            problems.append("{} is bound to {}".format(path, bound_path))

    anims = assets_of_class(ANIM_DIR, "AnimSequence")
    log("checking {} clip(s) under {}".format(len(anims), ANIM_DIR))

    wrong_skeleton = []
    lying_down = []
    frozen = []
    for path in sorted(anims):
        anim = unreal.EditorAssetLibrary.load_asset(path)
        if anim is None:
            problems.append("could not load {}".format(path))
            continue

        name = path.rsplit("/", 1)[-1]
        bound = anim.get_editor_property("skeleton")
        bound_path = package_path(bound.get_path_name()) if bound else "None"
        if bound_path != SKELETON_PATH:
            wrong_skeleton.append(name)

        # Upright means the pelvis sits along +Z; height leaking into Y means it plays on its back.
        hips = ""
        try:
            pose = unreal.AnimationLibrary.get_bone_pose_for_time(anim, "Hips", 0.0, False)
            loc = pose.translation
            hips = "hips=({:.1f},{:.1f},{:.1f})".format(loc.x, loc.y, loc.z)
            if loc.z <= abs(loc.y):
                lying_down.append(name)
        except Exception as ex:
            hips = "hips=<unreadable: {}>".format(ex)

        span = clip_motion_span(anim)
        if span < MIN_MOTION_SPAN:
            frozen.append(name)

        log("    {:<52} len={:.3f}s span={:.2f} {}".format(
            name, anim.get_editor_property("sequence_length"), span, hips))

    log("=== RESULT ===")
    for label, offenders in (
        ("clips bound to another skeleton", wrong_skeleton),
        ("clips playing lying down", lying_down),
        ("clips carrying no motion", frozen),
    ):
        if offenders:
            problems.append("{}: {}".format(label, offenders))
        else:
            log("OK: no {}".format(label))

    if problems:
        for item in problems:
            fail("FAILED: " + item)
    else:
        log("OK: rig intact after the hatless mesh swap; safe to commit")


main()
