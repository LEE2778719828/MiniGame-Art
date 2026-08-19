# Import the night-course hero rig and all of its animations onto ONE shared skeleton.
#
# Why the order below matters: every FBX in the pack carries the same Mixamo rig, but each
# export appended another generation of leaf terminator bones ("_end", "_end_end"), so the
# bone counts differ (47 / 48 / 53). Importing the files independently makes UE create one
# skeleton per file, and animations bound to different skeletons cannot play on one mesh.
#
# Slash_fast.fbx is imported first because its rig carries the widest leaf set (53 bones);
# every later file is imported animation-only against that skeleton, so no second skeleton
# is ever created and no duplicate mesh/material is produced.
#
# Run from the editor console:  py "<project>/Tools/ImportHeroRig.py"

import os
import unreal

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SOURCE_DIR = os.path.normpath(
    os.path.join(SCRIPT_DIR, "..", "..", "ArtSubmit", "Character", "mini_zhujue", "anim")
)

DEST_ROOT = "/Game/Night/Character"
DEST_ANIMS = DEST_ROOT + "/Anims"

# Slash_fast.fbx builds the rig, then every file (including it) is re-entered as
# animation-only so that all clips go through one identical code path.
#
# Slash.fbx is the artist's re-export from 0819 and replaces the original delivery, which was
# unusable twice over: 13 stray zero bytes ahead of the footer id made the SDK reject it during
# initialization, and once those were patched out every clip still imported frozen because all
# 16438 key arrays were zlib-compressed (a selective decision that Blender's exporter makes per
# array, so that file had been re-serialized by something else). The replacement clears both:
# no footer padding, 3328 raw arrays against 214 compressed, one animation stack instead of
# eight. Run Tools/ValidateArtFbx.py on any new delivery to check those two things up front.
MASTER_FBX = "Slash_fast.fbx"
ANIM_FBX = [
    "Slash_fast.fbx",
    "Slash.fbx",
    "Jump_noknife2.fbx",
    "Jump_noknife2_fast.fbx",
    "Sitting.fbx",
]
STATIC_FBX = "Knife.fbx"

MESH_NAME = "SK_Hero"
SKELETON_NAME = "SK_Hero_Skeleton"

# Blender writes these FBX with the "Armature" node carrying a -90 deg X rotation (its Z-up
# to FBX Y-up conversion). A skeletal mesh import bakes that node transform into the bind
# pose, so the mesh stands up; an animation-only import reads the Hips track in the armature's
# local space instead, which plays the character lying on its back. convert_scene does not
# cover this, so the rotation is re-applied here. UE Rotator roll is the X axis.
# Verified numerically: pelvis at t=0 moves from (0.6, -56.7, -1.0) to (0.6, -1.0, 56.7).
ANIM_IMPORT_ROLL = 90.0

# Acceptance threshold for "this clip actually animates": centimetres of translation or degrees
# of rotation, whichever channel moves most. An empty clip spans 0.00 on every channel, while the
# smallest real clip in the pack (the 200 ms slash) spans 8.4 in translation alone.
MIN_MOTION_SPAN = 0.5
MOTION_BONES = ("Hips", "RightHand", "LeftFoot", "Spine2")
MOTION_SAMPLES = (0.0, 0.15, 0.3, 0.45, 0.6, 0.75, 0.9, 0.99)

TAG = "[ImportHeroRig]"


def log(msg):
    unreal.log("{} {}".format(TAG, msg))


def fail(msg):
    unreal.log_error("{} {}".format(TAG, msg))


def package_path(object_path):
    """/Game/A/B.B -> /Game/A/B"""
    text = str(object_path)
    return text.split(".")[0]


def clip_motion_span(anim):
    """
    Widest movement of any sampled bone across the clip; 0 means the clip is a frozen pose.

    Both translation and rotation are measured. get_bone_pose_for_time returns the transform
    relative to the PARENT bone, and below the pelvis that translation is just a constant bone
    length -- an arm swing shows up purely as rotation. Translation alone would therefore only
    ever test the pelvis and would call a rotation-only clip frozen.

    Rotation is converted to a comparable scale by treating one degree of travel as one unit.
    """
    length = anim.get_editor_property("sequence_length")
    if length <= 0.0:
        return 0.0

    widest = 0.0
    for bone in MOTION_BONES:
        channels = [[] for _ in range(6)]
        for fraction in MOTION_SAMPLES:
            try:
                pose = unreal.AnimationLibrary.get_bone_pose_for_time(anim, bone, length * fraction, False)
            except Exception:
                channels = None
                break
            rotation = pose.rotation.rotator()
            for index, value in enumerate((pose.translation.x, pose.translation.y, pose.translation.z,
                                           rotation.roll, rotation.pitch, rotation.yaw)):
                channels[index].append(value)
        if channels is None:
            continue
        widest = max(widest, *(max(values) - min(values) for values in channels))
    return widest


def asset_class_name(asset_data):
    try:
        return str(asset_data.asset_class_path.asset_name)
    except Exception:
        return str(asset_data.asset_class)


def assets_of_class(directory, class_name, recursive=False):
    found = []
    if not unreal.EditorAssetLibrary.does_directory_exist(directory):
        return found
    for object_path in unreal.EditorAssetLibrary.list_assets(
        directory, recursive=recursive, include_folder=False
    ):
        data = unreal.EditorAssetLibrary.find_asset_data(object_path)
        if data and asset_class_name(data) == class_name:
            found.append(package_path(object_path))
    return found


def ensure_dir(directory):
    if not unreal.EditorAssetLibrary.does_directory_exist(directory):
        unreal.EditorAssetLibrary.make_directory(directory)


def run_import(filename, destination, options):
    source = os.path.join(SOURCE_DIR, filename)
    if not os.path.isfile(source):
        fail("missing source file: {}".format(source))
        return []

    task = unreal.AssetImportTask()
    task.filename = source
    task.destination_path = destination
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = options

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = [package_path(p) for p in task.imported_object_paths]
    log("{} -> {} asset(s)".format(filename, len(imported)))
    for path in imported:
        log("    {}".format(path))
    return imported


def apply_scene_conversion(import_data):
    """
    These clips are Blender/Mixamo exports (Z-up). The skeletal-mesh import data defaults
    to convert_scene=True, but the animation import data does not inherit it, so meshes came
    out standing while animation tracks came out rotated -90 deg about X (character lying
    down). Setting it explicitly on every import data keeps mesh and clips in one space.
    """
    if import_data is None:
        return
    for name, value in (
        ("convert_scene", True),
        ("force_front_x_axis", False),
        ("convert_scene_unit", False),
    ):
        try:
            import_data.set_editor_property(name, value)
        except Exception as ex:
            fail("could not set {} on {}: {}".format(name, type(import_data).__name__, ex))


def master_options():
    """Skeletal mesh + skeleton only; clips are imported separately as animation-only."""
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.import_animations = False
    options.import_materials = True
    options.import_textures = True
    options.create_physics_asset = False
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.original_import_type = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    apply_scene_conversion(options.skeletal_mesh_import_data)
    apply_scene_conversion(options.anim_sequence_import_data)
    return options


def anim_options(skeleton):
    """Animation only. Binding an explicit skeleton is what prevents a duplicate rig."""
    options = unreal.FbxImportUI()
    options.import_mesh = False
    options.import_as_skeletal = True
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.create_physics_asset = False
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.original_import_type = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.skeleton = skeleton
    apply_scene_conversion(options.anim_sequence_import_data)
    apply_scene_conversion(options.skeletal_mesh_import_data)

    try:
        options.anim_sequence_import_data.set_editor_property(
            "import_rotation", unreal.Rotator(roll=ANIM_IMPORT_ROLL, pitch=0.0, yaw=0.0)
        )
    except Exception as ex:
        fail("could not set import_rotation: {}".format(ex))

    return options


def static_options():
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_animations = False
    options.import_materials = True
    options.import_textures = True
    options.create_physics_asset = False
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.original_import_type = unreal.FBXImportType.FBXIT_STATIC_MESH
    return options


def rename(source, destination):
    if source == destination:
        return True
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        fail("rename target already exists: {}".format(destination))
        return False
    ok = unreal.EditorAssetLibrary.rename_asset(source, destination)
    log("rename {} -> {} : {}".format(source, destination, ok))
    return ok


def import_master():
    """Returns the loaded master skeleton, or None."""
    skeleton_path = "{}/{}".format(DEST_ROOT, SKELETON_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(skeleton_path):
        # Re-runs keep the existing rig so hand-authored data on it (sockets) survives.
        log("master skeleton already present, reusing {}".format(skeleton_path))
        return unreal.EditorAssetLibrary.load_asset(skeleton_path)

    imported = run_import(MASTER_FBX, DEST_ROOT, master_options())
    if not imported:
        fail("master import produced nothing; aborting")
        return None

    meshes = assets_of_class(DEST_ROOT, "SkeletalMesh")
    skeletons = assets_of_class(DEST_ROOT, "Skeleton")
    if len(meshes) != 1 or len(skeletons) != 1:
        fail(
            "expected exactly 1 mesh + 1 skeleton, got {} mesh / {} skeleton".format(
                len(meshes), len(skeletons)
            )
        )
        return None

    rename(meshes[0], "{}/{}".format(DEST_ROOT, MESH_NAME))
    rename(skeletons[0], "{}/{}".format(DEST_ROOT, SKELETON_NAME))

    skeleton_path = "{}/{}".format(DEST_ROOT, SKELETON_NAME)
    skeleton = unreal.EditorAssetLibrary.load_asset(skeleton_path)
    if skeleton is None:
        fail("could not load master skeleton at {}".format(skeleton_path))
    return skeleton


def clear_existing_anims():
    """Animation assets are fully regenerated on every run, so stale clips never linger."""
    for path in assets_of_class(DEST_ANIMS, "AnimSequence", recursive=True):
        ok = unreal.EditorAssetLibrary.delete_asset(path)
        log("delete {} : {}".format(path, ok))
        if not ok:
            fail("could not delete {} (asset editor open?)".format(path))


def main():
    if not os.path.isdir(SOURCE_DIR):
        fail("source dir missing: {}".format(SOURCE_DIR))
        return

    log("source dir: {}".format(SOURCE_DIR))
    ensure_dir(DEST_ROOT)

    skeleton = import_master()
    if skeleton is None:
        return

    ensure_dir(DEST_ANIMS)
    clear_existing_anims()
    for filename in ANIM_FBX:
        try:
            run_import(filename, DEST_ANIMS, anim_options(skeleton))
        except Exception as ex:
            fail("animation import failed for {}: {}".format(filename, ex))

    try:
        run_import(STATIC_FBX, DEST_ROOT, static_options())
    except Exception as ex:
        fail("static import failed for {}: {}".format(STATIC_FBX, ex))

    unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=False, recursive=True)

    # Self-check: a second skeleton anywhere under DEST_ROOT means the run went wrong.
    skeletons = assets_of_class(DEST_ROOT, "Skeleton", recursive=True)
    anims = assets_of_class(DEST_ROOT, "AnimSequence", recursive=True)
    meshes = assets_of_class(DEST_ROOT, "SkeletalMesh", recursive=True)
    statics = assets_of_class(DEST_ROOT, "StaticMesh", recursive=True)

    log("=== RESULT ===")
    log("skeletons     : {} {}".format(len(skeletons), skeletons))
    log("skeletal mesh : {} {}".format(len(meshes), meshes))
    log("static mesh   : {} {}".format(len(statics), statics))
    log("anim count    : {}".format(len(anims)))
    lying_down = []
    frozen = []
    for path in anims:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset:
            continue

        # Upright means the pelvis sits clearly along +Z. Height leaking into Y means the
        # character plays lying down; a negative Z means it plays upside down.
        hips = ""
        try:
            pose = unreal.AnimationLibrary.get_bone_pose_for_time(asset, "Hips", 0.0, False)
            loc = pose.translation
            hips = "  hips=({:.1f},{:.1f},{:.1f})".format(loc.x, loc.y, loc.z)
            if loc.z <= abs(loc.y):
                lying_down.append(path.rsplit("/", 1)[-1])
        except Exception as ex:
            hips = "  hips=<unreadable: {}>".format(ex)

        # A clip can import with the right name, length and frame count and still hold no
        # bone tracks at all; it then plays as a frozen pose and nothing upstream complains.
        # Sampling across the clip is the only thing that catches it.
        span = clip_motion_span(asset)
        if span < MIN_MOTION_SPAN:
            frozen.append(path.rsplit("/", 1)[-1])

        log(
            "    {}  len={:.3f}s  span={:.2f}  skel={}{}".format(
                path.rsplit("/", 1)[-1],
                asset.get_editor_property("sequence_length"),
                span,
                asset.get_editor_property("skeleton").get_name(),
                hips,
            )
        )

    if len(skeletons) == 1:
        log("OK: single shared skeleton")
    else:
        fail("FAILED: expected 1 skeleton, found {}".format(len(skeletons)))

    if lying_down:
        fail(
            "FAILED: {} clip(s) have pelvis height on Y, i.e. lying down: {}".format(
                len(lying_down), lying_down
            )
        )
    else:
        log("OK: all clips upright")

    if frozen:
        fail(
            "FAILED: {} clip(s) move less than {} units, i.e. imported empty: {}".format(
                len(frozen), MIN_MOTION_SPAN, frozen
            )
        )
    else:
        log("OK: all clips carry motion")


main()
